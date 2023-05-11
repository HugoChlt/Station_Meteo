#include "DHT.h"
#define tempMAX 20
#define humiMIN 5
#define rpmMAX 450
#define rayon_anemometre 0.011

#include <WiFi.h>
#include <PubSubClient.h>
#define MQTT_SERIAL_PUBLISH_CH "IF3B/PROJETStation/sensor/tx"
#define MQTT_SERIAL_RECEIVER_CH "IF3B/PROJETStation/sensor/rx" 

/* VARIABLES DHT11 */
DHT dht(26, DHT11);
int LED_R = 33;
int LED_G = 32;
int LED_Y = 27;

/* VARIABLES KY-024 */
int PIN_Digital = 25;
int rpm = 0;
int start = 0;
float periode = 0;
float vitesse = 0;

/* ---------------------------- CONNEXION WIFI ---------------------------- */
const char* ssid = "Moi";
const char* password = "Montlay21";
const char* mqtt_server = "mqtt.ci-ciad.utbm.fr";
#define mqtt_port 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_TEMPERATURE_OUT "IF3B/PROJETStation/sensor/temperature"
#define MQTT_HUMIDITE_OUT "IF3B/PROJETStation/sensor/humidité"
#define MQTT_WIND_OUT "IF3B/PROJETStation/sensor/vent"
#define MQTT_ALERTE_OUT "IF3B/PROJETStation/sensor/alerte"

WiFiClient wifiClient;
PubSubClient client(wifiClient);

int i = 0;
bool test = true;

void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.println("waiting pairing...");
    }
    randomSeed(micros());
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD)) {
      Serial.println("connected");
      //Once connected, publish an announcement...
      client.publish("IF3B/test/serialdata/tx", "reconnected");
      // ... and resubscribe
      client.subscribe(MQTT_SERIAL_RECEIVER_CH);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte *payload, unsigned int length) {
    Serial.println("-------new message from broker-----");
    Serial.print("channel:");
    Serial.println(topic);
    Serial.print("data:");
    Serial.write(payload, length);
    Serial.println();
}
/* ------------------------------------------------------------------------ */

void setup() {
  
  Serial.begin(115200);

  /* CONNEXION AU SERVEUR */
  Serial.setTimeout(500);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();

  /* DECLARATION DES PIN MODES */
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode (PIN_Digital, INPUT);
  
  /* DEMARRAGE DU DHT11 */
  dht.begin();
}

void publishSerialData(char *serialData){
  if (!client.connected()) {
    reconnect();
  }
  client.publish(MQTT_SERIAL_PUBLISH_CH, serialData);
}

void loop()
{ 
  /* WIFI */
  client.loop();
  if (Serial.available() > 0){
    char mun[501];
    memset(mun,0, 501);
    Serial.readBytesUntil( '\n',mun,500);
    publishSerialData(mun);
  }

  /* DECOMPTE DU NOMBRE DE TOURS PAR MINUTE ET DE LA VITESSE DE L'ANEMOMETRE */
  while(digitalRead(PIN_Digital) !=  1){
  }
  periode = millis()-start;
  start=millis();
  periode=periode/1000;
  rpm=60/periode*90;
  vitesse=0.12*3.14159*rayon_anemometre*rpm; /*formule permettant de déduire la vitesse du vent grâce à la vitesse de rotation de l'anémomètre et de son rayon*/
  
    /* AFFICHAGE DES RPM ET DE LA VITESSE DE L'ANEMOMETRE */
  Serial.print("RPM: ");
  Serial.print(rpm);
  Serial.println(" tr/min");
  Serial.print("Vitesse vent: ");
  Serial.print(vitesse);
  Serial.println(" km/h");

  /* AFFICHAGE DES DONNEES DU DHT11 */
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print("ºC ");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
  Serial.println("--------------------------------------------------------");

  /* ENVOI DES DONNEES AU SERVEUR MQTT */
  client.publish(MQTT_TEMPERATURE_OUT, String(temperature).c_str(), true);
  client.publish(MQTT_HUMIDITE_OUT, String(humidity).c_str(), true);
  client.publish(MQTT_WIND_OUT, String(vitesse).c_str(), true);

  delay(2000);
  
  /*
   * VERT : RAS
   * 
   * JAUNE : PRUDENCE
   * (température entre 0.74*tempMAX et 0.83*tempMAX)
   * OU
   * (humidité entre 1.5*humiMIN et 1.75*humiMIN )
   * OU
   * (température entre 0.67*tempMAX et 0.74*tempMAX ET humidité entre 1.75*humiMIN  et 2*humiMIN)
   * OU
   * (température entre 0.67*tempMAX et 0.74*tempMAX ET humidité entre 1.75*humiMIN  et 2*humiMIN ET
   * nombres de tours par minute entre 0.44*rpmMAX et 0.68*rpmMAX)
   * 
   * ROUGE CLIGNOTANT : ATTENTION
   * (température entre 0.83*tempMAX et tempMAX)
   * OU
   * (humidité entre 1.5*humiMIN et humiMIN)
   * OU
   * (température entre 0.74*tempMAX et 0.83*tempMAX ET humidité entre 1.5*humiMIN et 1.75*humiMIN )
   * OU
   * (température entre 0.74*tempMAX et 0.83*tempMAX ET humidité entre 1.5*humiMIN et 1.75*humiMIN ET
   * nombres de tours par minute entre 0.68*rpmMAX et rpmMAX)
   * 
   * ROUGE FIXE : DANGER
   * (temperature supérieure à tempMAX)
   * OU
   * (humidité inférieure à humiMIN)
   * OU
   * (température supérieure à 0.83*tempMAX ET humidité inférieure à 1.5*humiMIN)
   * OU
   * (température supérieure à 0.87*tempMAX ET humidité inférieure à 1.6*humiMIN ET nombre de tours par minute supérieur à rpmMAX)
   */

  if(temperature >= tempMAX || (humidity <= humiMIN) || (temperature >= 0.83*tempMAX && humidity <= 1.5*humiMIN)
  || (temperature >= 0.87*tempMAX && humidity <= 1.6*humiMIN && rpm > rpmMAX)){
    // LED ROUGE FIXE
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_R, HIGH);
    client.publish(MQTT_ALERTE_OUT, "DANGER", true);
  }else if((temperature > 0.83*tempMAX && temperature < tempMAX) || (humidity < 1.5*humiMIN && humidity > humiMIN)
  || ((temperature > 0.74*tempMAX && temperature < 0.83*tempMAX) && (humidity < 1.75*humiMIN  && humidity > 1.5*humiMIN))
  || ((temperature > 0.78*tempMAX && temperature < 0.87*tempMAX) && (humidity < 1.85*humiMIN  && humidity > 1.6*humiMIN))
  && (rpm > 0.68*rpmMAX && rpm < rpmMAX)){
    // LED ROUGE CLIGNOTANTE
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_R, HIGH);
    delay(580);
    digitalWrite(LED_R, LOW);
    delay(320);
    client.publish(MQTT_ALERTE_OUT, "ATTENTION", true);
  }else if ((temperature > 0.74*tempMAX && temperature < 0.83*tempMAX) || (humidity < 1.75*humiMIN  && humidity > 1.5*humiMIN)
  || ((temperature > 0.67*tempMAX && temperature < 0.74*tempMAX) && (humidity < 2*humiMIN && humidity > 1.75*humiMIN ))
  || ((temperature > 0.67*tempMAX && temperature < 0.74*tempMAX) && (humidity < 2*humiMIN && humidity > 1.75*humiMIN ) &&
  (rpm > 0.44*rpmMAX && rpm < 0.68*rpmMAX))){
    // LED JAUNE FIXE
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, HIGH);
    client.publish(MQTT_ALERTE_OUT,"FRAGILE", true);
  }else{
    // LED VERTE FIXE
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_G, HIGH);
    client.publish(MQTT_ALERTE_OUT, "OK", true);
  }
}
