#include "DHT.h"
#define tempMAX 29
#define humiMIN 5
#define rpmMAX 450
#define rayon_anemometre 0.0225
#define correctif 22
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

float delta = 0;
float delta_start = 0;

/* ---------------------------- CONNEXION WIFI ---------------------------- */
const char* ssid = "Tanguy's XR";
const char* password = "2002Tanguy!";
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

  /* DECOMPTE DU NOMBRE DE TOURS PAR MINUTE DE L'ANEMOMETRE */
  while(digitalRead(PIN_Digital)!=1){
    if(millis()-delta_start>=5000){
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
      delay(100);
  
      /* ENVOI DES DONNEES AU SERVEUR MQTT */
      client.publish(MQTT_TEMPERATURE_OUT, String(temperature).c_str(), true);
      client.publish(MQTT_HUMIDITE_OUT, String(humidity).c_str(), true);
      client.publish(MQTT_WIND_OUT, String(vitesse).c_str(), true);
  
  if(temperature >= tempMAX || (humidity <= humiMIN) || (temperature >= 0.83*tempMAX && humidity <= 1.5*humiMIN)
  || (temperature >= 0.87*tempMAX && humidity <= 1.6*humiMIN && rpm > rpmMAX)){
    // LED ROUGE FIXE
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_R, HIGH);
    client.publish(MQTT_ALERTE_OUT, "DANGER CRITIQUE", true);
  }else if((temperature > 0.83*tempMAX && temperature < tempMAX) || (humidity < 1.5*humiMIN && humidity > humiMIN)
  || ((temperature > 0.74*tempMAX && temperature < 0.83*tempMAX) && (humidity < 1.75*humiMIN  && humidity > 1.5*humiMIN))
  || ((temperature > 0.78*tempMAX && temperature < 0.87*tempMAX) && (humidity < 1.85*humiMIN  && humidity > 1.6*humiMIN))
  && (rpm > 0.68*rpmMAX && rpm < rpmMAX)){
    // LED JAUNE FIXE
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, HIGH);
    client.publish(MQTT_ALERTE_OUT, "LIMITE", true);
  }else if ((temperature > 0.74*tempMAX && temperature < 0.83*tempMAX) || (humidity < 1.75*humiMIN  && humidity > 1.5*humiMIN)
  || ((temperature > 0.67*tempMAX && temperature < 0.74*tempMAX) && (humidity < 2*humiMIN && humidity > 1.75*humiMIN ))
  || ((temperature > 0.67*tempMAX && temperature < 0.74*tempMAX) && (humidity < 2*humiMIN && humidity > 1.75*humiMIN ) &&
  (rpm > 0.44*rpmMAX && rpm < 0.68*rpmMAX))){
    // LED JAUNE FIXE
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, HIGH);
    client.publish(MQTT_ALERTE_OUT,"ATTENTION", true);
  }else{
    // LED VERTE FIXE
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_G, HIGH);
    client.publish(MQTT_ALERTE_OUT, "OK", true);
  }

      delta=millis()-delta_start;
      delta_start=millis();
    }
  }
  periode=millis()-start;
  start=millis();
  periode=periode/1000;
  rpm=60/periode;
  vitesse=correctif*3.6*(2*3.14159*rayon_anemometre)/periode;

  /* AFFICHAGE DES RPM DE L'ANEMOMETRE */
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
  delay(300);
  
  if(temperature >= tempMAX || (humidity <= humiMIN) || (temperature >= 0.83*tempMAX && humidity <= 1.5*humiMIN)
  || (temperature >= 0.87*tempMAX && humidity <= 1.6*humiMIN && rpm > rpmMAX)){
    // LED ROUGE FIXE
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_R, HIGH);
    client.publish(MQTT_ALERTE_OUT, "DANGER CRITIQUE", true);
  }else if((temperature > 0.83*tempMAX && temperature < tempMAX) || (humidity < 1.5*humiMIN && humidity > humiMIN)
  || ((temperature > 0.74*tempMAX && temperature < 0.83*tempMAX) && (humidity < 1.75*humiMIN  && humidity > 1.5*humiMIN))
  || ((temperature > 0.78*tempMAX && temperature < 0.87*tempMAX) && (humidity < 1.85*humiMIN  && humidity > 1.6*humiMIN))
  && (rpm > 0.68*rpmMAX && rpm < rpmMAX)){
    // LED JAUNE FIXE
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, HIGH);
    client.publish(MQTT_ALERTE_OUT, "LIMITE", true);
  }else if ((temperature > 0.74*tempMAX && temperature < 0.83*tempMAX) || (humidity < 1.75*humiMIN  && humidity > 1.5*humiMIN)
  || ((temperature > 0.67*tempMAX && temperature < 0.74*tempMAX) && (humidity < 2*humiMIN && humidity > 1.75*humiMIN ))
  || ((temperature > 0.67*tempMAX && temperature < 0.74*tempMAX) && (humidity < 2*humiMIN && humidity > 1.75*humiMIN ) &&
  (rpm > 0.44*rpmMAX && rpm < 0.68*rpmMAX))){
    // LED JAUNE FIXE
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_Y, HIGH);
    client.publish(MQTT_ALERTE_OUT,"ATTENTION", true);
  }else{
    // LED VERTE FIXE
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_G, HIGH);
    client.publish(MQTT_ALERTE_OUT, "OK", true);
  }
}
