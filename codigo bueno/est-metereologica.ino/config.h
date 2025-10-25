#ifndef CONFIG_H
#define CONFIG_H

// --- WiFi ---
#define WIFI_SSID       "MOVISTAR_8B3B"
#define WIFI_PASSWORD   "sUYPsMoDVZERqVFi2xWj"

// === DEFINICIONES DE IP ESTÁTICA (Necesarias para ESP32_Utils.hpp) ===
#define WIFI_IP         192,168,1,200   
#define WIFI_GATEWAY    192,168,1,1     
#define WIFI_SUBNET     255,255,255,0   
// ======================================================================

// --- MQTT ---
// IP de tu broker Mosquitto
#define MQTT_HOST       192,168,56,1    
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""
#define MQTT_CLIENT_ID  "ESP32_METEO_001"

// --- Zona Horaria (para NTP) ---
#define TIMEZONE        "Europe/Madrid"

// --- Topics base (opcional) ---
#define MQTT_BASE_TOPIC "estacion"

#endif