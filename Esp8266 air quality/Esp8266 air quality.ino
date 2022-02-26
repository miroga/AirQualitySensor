#include "Adafruit_SGP40.h"
#include "Adafruit_SHTC3.h"
#include "Adafruit_PM25AQI.h"
#include "ESP8266WiFi.h"
#include "PubSubClient.h"
#include "ArduinoJson.h"

// WiFi settings
#define WIFI_SSID                     "WIFI_SSID"
#define WIFI_PASS                     "WIFI_PASS"

// MQTT Settings
#define CLIENT_ID                     "air-quality"
#define MQTT_SERVER                   "MQTT_SERVER"
#define MQTT_PORT                     1883
#define STATE_TOPIC                   "home/livingroom/airquality"
#define JSON_ATTRIBUTES_TOPIC         "home/livingroom/airquality/attributes"
#define AVAILABILITY_TOPIC            "home/livingroom/airquality/available"
#define MQTT_USERNAME                 "MQTT_USERNAME"
#define MQTT_PASSWORD                 "MQTT_PASSWORD"

// ESP8266 WiFi
WiFiClient wifiClient;
// MQTT
PubSubClient client(wifiClient);

// Sensors
Adafruit_SGP40 sgp;
Adafruit_SHTC3 shtc3;
Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

void setup() {
  setupSerial();

  setupWifi();
  setupMqtt();

  setupSensors();
}

void loop() {
  int32_t voc_index;
  sensors_event_t humidity, temp;
  PM25_AQI_Data data;

  shtc3.getEvent(&humidity, &temp);
  // Serial.print("Temp *C = "); Serial.print(temp.temperature); Serial.print("\t\t");
  // Serial.print("Hum. % = "); Serial.println(humidity.relative_humidity);

  // uint16_t sraw;
  // sraw = sgp.measureRaw(temp.temperature, humidity.relative_humidity);
  // Serial.print("SGP40 raw measurement: ");
  // Serial.println(sraw);

  voc_index = sgp.measureVocIndex(temp.temperature, humidity.relative_humidity);
  // Serial.print("Voc Index: ");
  // Serial.println(voc_index);

  if (! aqi.read(&data)) {
    // Serial.println("Could not read from AQI");
    delay(500);  // try again in a bit!
    return;
  }
  // Serial.println("AQI reading success");

  // Serial.println();
  // Serial.println(F("---------------------------------------"));
  // Serial.println(F("Concentration Units (standard)"));
  // Serial.println(F("---------------------------------------"));
  // Serial.print(F("PM 1.0: ")); Serial.print(data.pm10_standard);
  // Serial.print(F("\t\tPM 2.5: ")); Serial.print(data.pm25_standard);
  // Serial.print(F("\t\tPM 10: ")); Serial.println(data.pm100_standard);
  // Serial.println(F("Concentration Units (environmental)"));
  // Serial.println(F("---------------------------------------"));
  // Serial.print(F("PM 1.0: ")); Serial.print(data.pm10_env);
  // Serial.print(F("\t\tPM 2.5: ")); Serial.print(data.pm25_env);
  // Serial.print(F("\t\tPM 10: ")); Serial.println(data.pm100_env);
  // Serial.println(F("---------------------------------------"));
  // Serial.print(F("Particles > 0.3um / 0.1L air:")); Serial.println(data.particles_03um);
  // Serial.print(F("Particles > 0.5um / 0.1L air:")); Serial.println(data.particles_05um);
  // Serial.print(F("Particles > 1.0um / 0.1L air:")); Serial.println(data.particles_10um);
  // Serial.print(F("Particles > 2.5um / 0.1L air:")); Serial.println(data.particles_25um);
  // Serial.print(F("Particles > 5.0um / 0.1L air:")); Serial.println(data.particles_50um);
  // Serial.print(F("Particles > 10 um / 0.1L air:")); Serial.println(data.particles_100um);
  // Serial.println(F("---------------------------------------"));
  // Serial.println(F("---------------------------------------"));
  
  createAndSendMessage(temp.temperature, humidity.relative_humidity, voc_index, data);
  createAndSendAttributesMessage(data);

  client.loop();

  delay(30000);
}

void setupSerial(){
  Serial.begin(115200);
  Serial.println();
}

void setupWifi(){
   WiFi.begin(WIFI_SSID, WIFI_PASS);
 
  // Connecting to WiFi...
  // Serial.print("Connecting to ");
  // Serial.print(WIFI_SSID);
  // Loop continuously while WiFi is not connected
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    // Serial.print(".");
  }

  // Connected to WiFi
  // Serial.println();
  // Serial.print("Connected! IP address: ");
  // Serial.println(WiFi.localIP());
}

void setupMqtt(){
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setKeepAlive(70);
}

void setupSensors(){
  if (!sgp.begin()){
    Serial.println("SGP40 sensor not found :(");
    while (1);
  }

  if (!shtc3.begin()) {
    Serial.println("Couldn't find SHTC3");
    while (1);
  }

  if (!aqi.begin_I2C()) {
    Serial.println("Could not find PM 2.5 sensor!");
    while (1) delay(10);
  }
}

void createAndSendMessage(float temperature, float humidity, int32_t vocIndex, PM25_AQI_Data aqiData){
  StaticJsonDocument<128> doc;
  JsonObject message = doc.to<JsonObject>();
   
  message["temperature"] = temperature;
  message["humidity"] = humidity;
  message["vocindex"] = vocIndex;
  message["pmindex"] = getAirQualityPMIndex(aqiData);
  message["pm10env"] = aqiData.pm10_env;
  message["pm25env"] = aqiData.pm25_env;
  message["pm100env"] = aqiData.pm100_env;

  // Serial.print("Less overhead JSON message size: ");
  // Serial.println(measureJson(doc));
  
  // Serial.println("\nPretty JSON message: ");
  // serializeJsonPretty(doc, Serial);
  // Serial.println();

  if (!client.connected()) {
    reconnect();
  }

  char buffer[128];
  size_t n = serializeJson(doc, buffer);
  client.publish(STATE_TOPIC, buffer, n);

  // client.loop();
}

void createAndSendAttributesMessage(PM25_AQI_Data aqiData){
  StaticJsonDocument<256> doc;
  JsonObject aqi = doc.to<JsonObject>();

  // aqi["pm10standard"] = aqiData.pm10_standard;   // from datasheet: "should be used in the factory environment"
  // aqi["pm25standard"] = aqiData.pm25_standard;   // from datasheet: "should be used in the factory environment"
  // aqi["pm100standard"] = aqiData.pm100_standard; // from datasheet: "should be used in the factory environment"

  // aqi["pm10env"] = aqiData.pm10_env;
  // aqi["pm25env"] = aqiData.pm25_env;
  // aqi["pm100env"] = aqiData.pm100_env;
  
  aqi["particles03um"] = aqiData.particles_03um;
  aqi["particles05um"] = aqiData.particles_05um;
  aqi["particles10um"] = aqiData.particles_10um;
  aqi["particles25um"] = aqiData.particles_25um;
  aqi["particles50um"] = aqiData.particles_50um;
  aqi["particles100um"] = aqiData.particles_100um;

  // Serial.print("Less overhead JSON message size: ");
  // Serial.println(measureJson(aqi));
  
  // Serial.println("\nPretty JSON message: ");
  // serializeJsonPretty(aqi, Serial);
  // Serial.println();

  if (!client.connected()) {
    reconnect();
  }

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  client.publish(JSON_ATTRIBUTES_TOPIC, buffer, n);

  // client.loop();
}

void reconnect() {
  while (!client.connected()) {       // Loop until connected to MQTT server
    //Serial.print("Attempting MQTT connection...");
    
    if (client.connect(CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, AVAILABILITY_TOPIC, 2, true, "offline")) {       //Connect to MQTT server
      //Serial.println("connected"); 
      client.publish(AVAILABILITY_TOPIC, "online", true);         // Once connected, publish online to the availability topic
    } else {
      // Serial.print("failed, rc=");
      // Serial.print(client.state());
      // Serial.println(" try again in 5 seconds");
      delay(5000);  // Will attempt connection again in 5 seconds
    }
  }
}

int getAirQualityPMIndex(PM25_AQI_Data aqiData){
  // https://en.wikipedia.org/wiki/Air_quality_index
  float pm100 = getQualityPM100Index(aqiData.pm100_env);
  float pm25 = getQualityPM25Index(aqiData.pm25_env);
  int index = round((pm100 + pm25) / 2);

  // Serial.print("PM10: ");
  // Serial.print(pm100);
  // Serial.print("; PM2.5: ");
  // Serial.print(pm25);
  // Serial.print("; Index: ");
  // Serial.println(index);

  return index;
}

float getQualityPM100Index(int value){
  if(value >= 0 && value < 25){
    return 1.0; // good
  } else if(value >= 25 && value <= 50){
    return 2.0; // satisfactory
  } else if(value >= 51 && value <= 90){
    return 3.0; // moderate
  } else if(value >= 91 && value <= 180){
    return 4.0; // poor
  }
  
  return 5.0; // very poor
}

float getQualityPM25Index(int value){
  if(value >= 0 && value < 15){
    return 1.0; // good
  } else if(value >= 15 && value <= 30){
    return 2.0; // satisfactory
  } else if(value >= 31 && value <= 55){
    return 3.0; // moderate
  } else if(value >= 56 && value <= 110){
    return 4.0; // poor
  }
  
  return 5.0; // very poor
}
