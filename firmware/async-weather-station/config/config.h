#ifndef WEATHER_STATION_CONFIG_H
#define WEATHER_STATION_CONFIG_H

// --- WiFi ---
#define WIFI_SSID       ""
#define WIFI_PASSWORD   ""

// === DEFINICIONES DE IP ESTÁTICA (Necesarias para ESP32_Utils.hpp) ===
#define WIFI_IP         192,168,1,200
#define WIFI_GATEWAY    192,168,1,1
#define WIFI_SUBNET     255,255,255,0
// ======================================================================

// --- MQTT ---
// IP de tu broker Mosquitto (formato: 192,168,1,10)
#define MQTT_HOST       192,168,56,1
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""
#define MQTT_CLIENT_ID  "ESP32_METEO_001"
// Topic único para publicar y recibir mensajes MQTT
#define MQTT_TOPIC      "sensors/street_1253/WT_001"
#define MQTT_QOS        1

// --- Zona Horaria (para NTP) ---
#define TIMEZONE        "Europe/Madrid"

#endif  // WEATHER_STATION_CONFIG_H
