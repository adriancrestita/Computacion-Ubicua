#ifndef CONFIG_H
#define CONFIG_H

// --- WiFi ---
#define WIFI_SSID       "Tu_SSID"
#define WIFI_PASSWORD   "Tu_PASSWORD"

// --- MQTT ---
#define MQTT_HOST       "test.mosquitto.org"
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""
#define MQTT_CLIENT_ID  "ESP32_METEO_001"
#define MQTT_TOPIC_SUB  "esp32/prueba/#"
#define MQTT_TOPIC_PUB  "esp32/prueba/enviar"

// --- Zona Horaria (para NTP) ---
#define TIMEZONE        "Europe/Madrid"

#endif
