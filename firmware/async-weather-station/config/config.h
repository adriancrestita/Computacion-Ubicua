#ifndef WEATHER_STATION_CONFIG_H
#define WEATHER_STATION_CONFIG_H

#include <IPAddress.h>

// --- WiFi ---
const char* ssid     = "cubicua";
const char* password = "";
const char* hostname = "ESP32_METEO_001";

IPAddress ip(192, 168, 1, 200);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// --- MQTT ---
// IP de tu broker Mosquitto (formato: 192,168,1,10)
#define MQTT_HOST       172,22,92,67
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""
#define MQTT_CLIENT_ID  "ESP32_METEO_001"
// Topic único para publicar y recibir mensajes MQTT
#define MQTT_TOPIC      "sensors/street_1253/WT_001"
#define MQTT_QOS        1

#endif  // WEATHER_STATION_CONFIG_H
