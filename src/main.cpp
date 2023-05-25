#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
//#include <OneWire.h>
//#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <Adafruit_BMP085.h>
#include <SPI.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

#define DHT_A_SENSOR_PIN  4 // A= Außen sensor
#define DHT_I_SENSOR_PIN  12 //I= innen sensor
#define DHT_SENSOR_TYPE DHT22

DHT dht_A_sensor(DHT_A_SENSOR_PIN, DHT_SENSOR_TYPE);
DHT dht_I_sensor(DHT_I_SENSOR_PIN, DHT_SENSOR_TYPE);

#define Led 2
#define relay 5
#define torzu 27 //Pin auf dem Strom fließ wenn das Tor nicht zu ist 
#define torauf 25 //Pin auf dem Strom fließ wenn Tor nicht offen ist
//------------------------------------------------------------------------> NC Endschalter

#define LOCAL_ALTITUDE (1070.0F)

#define I2C_SDA 21 // bmp180
#define I2C_SCL 22 // bmp180

Adafruit_BMP085 bmp;


//const char* ssid = "Christian-Simone-2.4G"; //Außen
//const char* ssid = "Christian-Simone-d"; //Keller
const char* ssid = "Christian-Simone-G"; //Garage
const char* password = "Internetio22";
const char* mqtt_server = "192.168.1.159";

WiFiClient Wclient;
PubSubClient mqtt_client(Wclient);

//Wertte Variablen
float tempC_I;
float humi_I;
float humi_A;

long now = millis();
long nowc = millis();


//Anfangswert der Variablen
int torzu_ = 1;
int torauf_ = 1;

void setup_WIFI(){
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(Led, HIGH);
}

void callback(String topic, byte* message, unsigned int Lenght){
Serial.print("Message arrived on topic: ");
Serial.print(topic);
Serial.print(". Message: ");
String messageTemp;

for (int i = 0; i < Lenght; i++){
  Serial.print((char)message[i]);
  messageTemp+= (char)message[i];
}
Serial.println();

if(String(topic) == "/triggertor"){ 
  if(messageTemp == "triggeron"){ 
    digitalWrite(relay, HIGH); // Wenn die Nachricht angekommen ist dann soll der Pin angesteuert Werden
    delay(200);
    mqtt_client.publish("/triggertor", "triggeroff"); // Rückmeldung dass trigger angekommen ist.
    digitalWrite(relay, LOW); // Rücksetzung des Pins
    mqtt_client.unsubscribe("/triggertor")
    mqtt_client.subscribe("/triggertor");
     }
  else if(messageTemp == "triggeroff"){ digitalWrite(relay, LOW); }
}
Serial.println();
}

void reconnect(){
  while(!mqtt_client.connected()){
    Serial.print("Attempting MQTT connection... ");

    if (mqtt_client.connect("ESP8266Client1")){
      Serial.println("connected");
      digitalWrite(Led, HIGH);
    } else{
      Serial.print("failed, rc= ");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void data_Ma(){

  humi_A = dht_A_sensor.readHumidity();

  if ( isnan(humi_A)) { Serial.println("Failed to read from DHT sensor! a");} 
  else {
  Serial.println(String(humi_A).c_str());
  mqtt_client.publish("/hum/außen/gar", String(humi_A).c_str());
  }
  Serial.println(String(bmp.readTemperature()).c_str());
  mqtt_client.publish("/temp/außen/gar", String(bmp.readTemperature()).c_str());
  
    Serial.println("Data Send_a");
}

void data_Mb(){
   humi_I  = dht_I_sensor.readHumidity();
   tempC_I = dht_I_sensor.readTemperature();

  if ( isnan(tempC_I) ||  isnan(humi_I)) { Serial.println("Failed to read from DHT sensor!"); } 
  else {
    Serial.println(String(humi_I).c_str());
    Serial.println(String(tempC_I).c_str());

    mqtt_client.publish("/hum/innen/gar", String(humi_I).c_str());
    mqtt_client.publish("/temp/innen/gar", String(tempC_I).c_str());
  }
  Serial.println("Data Send_b");
}

void data_Mc(){

  int wert = 0;
   
  wert = bmp.readSealevelPressure(LOCAL_ALTITUDE);//dieser Wert wird benötigt

  
  Serial.println(String(wert).c_str());
  mqtt_client.publish("/pres/außen/gar/s", String(wert).c_str());
  
  Serial.println("Data Send_c");
}

void setup() {

  pinMode(Led, OUTPUT);//Index Lampe of alles verbunden ist
  pinMode(relay, OUTPUT);

  pinMode(torzu, INPUT);
  pinMode(torauf, INPUT);
  Serial.begin(115200);

  setup_WIFI();
  if (!bmp.begin()) {
	  Serial.println("Could not find a valid BMP085/BMP180 sensor, check wiring!");
	  while (1) {}
  }
  dht_A_sensor.begin();
  dht_I_sensor.begin();

  torzu_ = digitalRead(torzu);
  torauf_ = digitalRead(torauf);

  
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(callback);

  reconnect();
  mqtt_client.subscribe("/triggertor");

  data_Ma();
  data_Mb();
  data_Mc();
}


void loop() {
  if(!mqtt_client.connected()){
  digitalWrite(Led, LOW);
  reconnect();
    mqtt_client.unsubscribe("/triggertor")
    mqtt_client.subscribe("/triggertor");
  }

  if(WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(Led, LOW);
    setup_WIFI();
    mqtt_client.unsubscribe("/triggertor")
    mqtt_client.subscribe("/triggertor");
  }
  
  mqtt_client.loop();

  if(millis() - now > 300000){ // alle 5 minuten
    now = millis();
    data_Ma();
    data_Mb(); 
  }

  if(millis() - nowc > 3600000){ // jede Stunde
    nowc = millis();
    data_Mc();
    mqtt_client.unsubscribe("/triggertor")
    mqtt_client.subscribe("/triggertor");
  }


//Status auszulesen
 if(digitalRead(torzu) != torzu_){
    torzu_ = digitalRead(torzu);
    Serial.println(torzu_);
    if(torzu_ == 1){ mqtt_client.publish("/torstatus", "Öffnen.."); }
    else { mqtt_client.publish("/torstatus", "Geschlossen"); }
  }

  if(digitalRead(torauf) != torauf_){
    torauf_ = digitalRead(torauf);
    Serial.println(torauf_);
    if(torauf_ == 1){ mqtt_client.publish("/torstatus", "Schließen.."); }
    else{ mqtt_client.publish("/torstatus", "Geöffnet"); }
  } 
}
