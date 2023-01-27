#include "Adafruit_SGP40.h"
#include "Adafruit_PM25AQI.h"
#include "Adafruit_VEML7700.h"
#include "SensirionI2CScd4x.h"
#include "Wire.h"
#include "ESP8266WiFi.h"
#include "PubSubClient.h"
#include "ArduinoJson.h"
#include "LOLIN_EPD.h"
#include "Adafruit_GFX.h"
#include "config.h"
#include "Adafruit_SHTC3.h"

#define HOSTNAME                      "Air Quality Sensor"

// ESP8266 WiFi
WiFiClient wifiClient;
// MQTT
PubSubClient client(wifiClient);

// Sensors
Adafruit_SGP40 sgp;
Adafruit_PM25AQI aqi = Adafruit_PM25AQI();
Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();
Adafruit_VEML7700 veml = Adafruit_VEML7700();
SensirionI2CScd4x scd41;

// E-Ink Display
LOLIN_SSD1680 EPD(250, 122, EPD_DC, EPD_RST, EPD_CS, EPD_BUSY);

const unsigned long MeasurementInterval = 10*60*1000; // 10 minutes
unsigned long previousMillis = 0;
bool isFirstLoopRun = true;

void setup()
{
  setupSerial();

  setupWifi();
  setupMqtt();

  setupSensors();

  setupEpd();

  Serial.println("Setup done.");
}

void loop()
{
  unsigned long currentMillis = millis();
  unsigned long elapsedMillis = currentMillis - previousMillis;

  if(isFirstLoopRun)
  {
    takeMeasurements();

    isFirstLoopRun = false;
  }

  if (elapsedMillis >= MeasurementInterval)
  {
    previousMillis = currentMillis;

    takeMeasurements();
  }

  client.loop();
  delay(1000);
}

void takeMeasurements()
{
  int32_t voc_index;
  PM25_AQI_Data data;
  uint16_t co2;
  float scd41Temperature;
  float scd41Humidity;
  sensors_event_t humidity, temp;
  float lux = veml.readLux();

  scd41.measureSingleShot();
  scd41.readMeasurement(co2, scd41Temperature, scd41Humidity);
  
  shtc3.getEvent(&humidity, &temp);

  voc_index = sgp.measureVocIndex(temp.temperature, humidity.relative_humidity);

  if (! aqi.read(&data))
  {
    // Serial.println("Could not read from AQI");
    delay(500);  // try again in a bit!
    return;
  }

  processValues(temp.temperature, humidity.relative_humidity, voc_index, data, co2, lux);
}

void processValues(float temperature, float humidity, int32_t vocIndex, PM25_AQI_Data aqiData, uint16_t co2, float lux)
{
  displayValues(temperature, humidity, vocIndex, aqiData, co2);

  createAndSendMessage(temperature, humidity, vocIndex, aqiData, co2, lux);
  createAndSendAttributesMessage(aqiData);
}

void setupSerial()
{
  Serial.begin(115200);
  Serial.println();
}

void setupWifi()
{
  WiFi.setHostname(HOSTNAME);
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
  // Serial.println(WiFi.localIP());
}

void setupMqtt()
{
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  int keepAlive = 10*60*2.5; // 25 minutes
  client.setKeepAlive(keepAlive);
}

void setupSensors()
{
  Wire.begin();
  scd41.begin(Wire);
  scd41.stopPeriodicMeasurement();

  if (! shtc3.begin()) {
    Serial.println("Couldn't find SHTC3");
    while (1) delay(1);
  }

  if (!sgp.begin())
  {
    Serial.println("SGP40 sensor not found :(");
    while (1);
  }

  if (!aqi.begin_I2C())
  {
    Serial.println("Could not find PM 2.5 sensor!");
    while (1) delay(10);
  }

  if (!veml.begin())
  {
    Serial.println("VEML7700 sensor not found");
    while (1);
  }
}

void setupEpd()
{
  EPD.begin();
}

void createAndSendMessage(float temperature, float humidity, int32_t vocIndex, PM25_AQI_Data aqiData, uint16_t co2, float lux)
{
  StaticJsonDocument<256> doc;
  JsonObject message = doc.to<JsonObject>();
   
  message["temperature"] = temperature;
  message["humidity"] = humidity;
  message["co2"] = co2;
  message["vocindex"] = vocIndex;
  message["lux"] = lux;
  message["pmindex"] = getAirQualityPMIndex(aqiData);
  message["pm10env"] = aqiData.pm10_env;
  message["pm25env"] = aqiData.pm25_env;
  message["pm100env"] = aqiData.pm100_env;

  if (!client.connected())
  {
    reconnect();
  }

  // serializeJson(doc, Serial);

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  client.publish(STATE_TOPIC, buffer, n);
}

void createAndSendAttributesMessage(PM25_AQI_Data aqiData)
{
  StaticJsonDocument<256> doc;
  JsonObject attributes = doc.to<JsonObject>();

  // attributes["pm10standard"] = aqiData.pm10_standard;   // from datasheet: "should be used in the factory environment"
  // attributes["pm25standard"] = aqiData.pm25_standard;   // from datasheet: "should be used in the factory environment"
  // attributes["pm100standard"] = aqiData.pm100_standard; // from datasheet: "should be used in the factory environment"
  
  attributes["particles03um"] = aqiData.particles_03um;
  attributes["particles05um"] = aqiData.particles_05um;
  attributes["particles10um"] = aqiData.particles_10um;
  attributes["particles25um"] = aqiData.particles_25um;
  attributes["particles50um"] = aqiData.particles_50um;
  attributes["particles100um"] = aqiData.particles_100um;

  if (!client.connected())
  {
    reconnect();
  }

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  client.publish(JSON_ATTRIBUTES_TOPIC, buffer, n);
}

void reconnect()
{
  while (!client.connected()) {       // Loop until connected to MQTT server
    //Serial.print("Attempting MQTT connection...");
    
    if (client.connect(CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, AVAILABILITY_TOPIC, 2, true, "offline")) {       //Connect to MQTT server
      //Serial.println("connected"); 
      client.publish(AVAILABILITY_TOPIC, "online", true);         // Once connected, publish online to the availability topic
      client.subscribe(CALIBRATION_TOPIC);
    } else {
      // Serial.print("failed, rc=");
      // Serial.print(client.state());
      // Serial.println(" try again in 5 seconds");
      delay(5000);  // Will attempt connection again in 5 seconds
    }
  }
}

int getAirQualityPMIndex(PM25_AQI_Data aqiData)
{
  // https://en.wikipedia.org/wiki/Air_quality_index
  float pm100 = getQualityPM100Index(aqiData.pm100_env);
  float pm25 = getQualityPM25Index(aqiData.pm25_env);
  int index = round((pm100 + pm25) / 2);

  return index;
}

float getQualityPM100Index(int value)
{
  if(value >= 0 && value < 25)
  {
    return 1.0; // good
  }
  else if(value >= 25 && value <= 50)
  {
    return 2.0; // satisfactory
  }
  else if(value >= 51 && value <= 90)
  {
    return 3.0; // moderate
  }
  else if(value >= 91 && value <= 180)
  {
    return 4.0; // poor
  }
  
  return 5.0; // very poor
}

float getQualityPM25Index(int value)
{
  if(value >= 0 && value < 15)
  {
    return 1.0; // good
  }
  else if(value >= 15 && value <= 30)
  {
    return 2.0; // satisfactory
  }
  else if(value >= 31 && value <= 55)
  {
    return 3.0; // moderate
  }
  else if(value >= 56 && value <= 110)
  {
    return 4.0; // poor
  }
  
  return 5.0; // very poor
}

void displayValues(float temp, float hum, int voc, PM25_AQI_Data aqiData, int co2)
{
  EPD.clearBuffer();
  EPD.fillScreen(EPD_WHITE);
  EPD.drawLine(84, 0, 84, EPD.height(), EPD_BLACK);
  EPD.drawLine(166, 0, 166, EPD.height(), EPD_BLACK);
  EPD.drawLine(EPD.width(), 61, 84, 61, EPD_BLACK);

  EPD.setTextColor(EPD_BLACK);

  printFirstRow(temp, hum);
  printSecondRow(voc, getAirQualityPMIndex(aqiData));
  printCo2(co2);

  EPD.display();
}

void printFirstRow(float temp, float hum)
{
  uint16_t headerY = 6;
  uint16_t valueY = headerY + 30;
  EPD.setTextSize(2);
  
  // title
  EPD.setCursor(96, headerY);
  EPD.print("Tepl.");
  // value
  EPD.setCursor(95, valueY);
  EPD.print(temp, 2);
  
  // title
  EPD.setCursor(181, headerY);
  EPD.print("Vlhk.");
  // value
  EPD.setCursor(174, valueY);
  EPD.print(hum);
  EPD.print("%");
}

void printSecondRow(uint16_t voc, uint8_t pmIndex)
{
  uint32_t headerY = 72;
  uint16_t valueY = headerY + 30;
  EPD.setTextSize(2);
  
  // title
  EPD.setCursor(109, headerY);
  EPD.print("VOC");

  uint32_t vocPositionX = 109;
  if (voc < 100)
  {
    vocPositionX = 116;
  }
  else if (voc > 1000)
  {
    vocPositionX = 102;
  }

  // value
  EPD.setCursor(vocPositionX, valueY);
  EPD.print(voc);
  
  // title
  EPD.setCursor(184, headerY);
  EPD.print("PM");
  EPD.setCursor(EPD.getCursorX(), headerY + 7);
  EPD.setTextSize(1);
  EPD.print("index");
  // value
  EPD.setTextSize(2);
  EPD.setCursor(204, valueY);
  EPD.print(pmIndex);
}

void printCo2(uint16_t co2)
{
  uint32_t headerY = 20;

  // title
  EPD.setTextSize(3);
  EPD.setCursor(17, headerY);
  EPD.print("CO");
  EPD.setTextSize(2);
  EPD.setCursor(EPD.getCursorX(), headerY + 12);
  EPD.print(2);

  // value
  int positionX = 6;
  if(co2 < 1000)
  {
    positionX = 14;
  }
  EPD.setTextSize(3);
  EPD.setCursor(positionX, 82);
  EPD.print(co2);
}

void callback(char* topic, byte* payload, unsigned int length)
{
  StaticJsonDocument <256> doc;
  deserializeJson(doc, payload);
  // serializeJson(doc, Serial);

  if (strcmp(topic, CALIBRATION_TOPIC) == 0)
  {
    Serial.println("Starting calibration...");

    long co2Correction = doc["co2"];
    long altitudeCorrection = doc["altitude"];
    
    bool isCalibrated = calibrateScd41(co2Correction, altitudeCorrection);

    if(isCalibrated)
    {
      Serial.println("Calibration done.");
    }
    else
    {
      Serial.println("Calibration could not be performed.");
    }
  }
}

bool calibrateScd41(long co2Correction, long altitudeCorrection)
{
  // int stopCode = scd41.stopPeriodicMeasurement();
  // // delay(1000);

  // if(stopCode != 0)
  // {
  //   Serial.println("Periodic measurements could not be stopped.");

  //   return false;
  // }

  bool altitudeIsSet = false;
  bool co2CorrectionIsSet = false;

  if(altitudeCorrection >= 0)
  {
    scd41.setSensorAltitude(237);
    altitudeIsSet = true;
  }

  if(co2Correction >= 400)
  {
    uint16_t correction;
    scd41.performForcedRecalibration(410, correction);
    co2CorrectionIsSet = true;

    Serial.print("Correction: ");
    Serial.println(correction);
  }

  if(altitudeIsSet || co2CorrectionIsSet)
  {
    int settingsSavedCode = scd41.persistSettings();

    if(settingsSavedCode != 0)
    {
      Serial.println("SCD41 could not save calibration settings.");
      return false;
    }
    else
    {
      Serial.println("SCD41 saved calibration settings.");
    }
  }
  else
  {
    Serial.println("No calibration performed.");
    
    return false;    
  }

  // bool started = scd41.startPeriodicMeasurement();

  // if(!started)
  // {
  //   Serial.println("Could not start periodic measurements.");
  //   return false;
  // }

  return true;
}