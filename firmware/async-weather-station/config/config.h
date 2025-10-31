#ifndef WEATHER_STATION_CONFIG_H
#define WEATHER_STATION_CONFIG_H

#include <IPAddress.h>

// --- WiFi ---
const char* ssid     = "DIGI-R5mE";
const char* password = "hEypg6XW";
const char* hostname = "";

// Nota: los parámetros IP permanecen solo como referencia; no se utilizan porque la conexión es por DHCP.
IPAddress ip(192, 168, 1, 200);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// --- MQTT ---
// IP de tu broker Mosquitto (formato: 192,168,1,10)
#define MQTT_HOST       "192.168.1.131"
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""
#define MQTT_CLIENT_ID  ""
// Topic único para publicar y recibir mensajes MQTT
#define MQTT_BASE_TOPIC "sensors/street_1253/WT_001"
#define MQTT_TOPIC      "sensors/street_1253/WT_001"
#define MQTT_QOS        1

#endif  // WEATHER_STATION_CONFIG_H
