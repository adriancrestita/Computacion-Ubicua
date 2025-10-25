#ifndef CONFIG_H
#define CONFIG_H

// --- WiFi ---
#define WIFI_SSID       "cubicua"
#define WIFI_PASSWORD   ""

// --- MQTT ---
#define MQTT_HOST       "192.168.1.100"    // IP o dominio del broker
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""
#define MQTT_CLIENT_ID  "ESP32_METEO_001"

// --- Zona Horaria (para NTP) ---
#define TIMEZONE        "Europe/Madrid"

// --- Topics base (opcional) ---
#define MQTT_BASE_TOPIC "estacion"

#endif