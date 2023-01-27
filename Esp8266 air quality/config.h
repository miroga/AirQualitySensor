// E-Ink Display settings
#define EPD_CS D0
#define EPD_DC D8
#define EPD_RST -1
#define EPD_BUSY -1

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
#define CALIBRATION_TOPIC             "home/livingroom/airquality/calibrate"
#define MQTT_USERNAME                 "MQTT_USERNAME"
#define MQTT_PASSWORD                 "MQTT_PASSWORD"