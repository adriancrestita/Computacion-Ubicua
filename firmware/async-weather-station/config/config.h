#ifndef WEATHER_STATION_CONFIG_H
#define WEATHER_STATION_CONFIG_H

// --- WiFi ---
#define WIFI_SSID       "DIGI-R5mE"
#define WIFI_PASSWORD   "hEypg6XW"

// --- MQTT ---
// IP de tu broker Mosquitto (formato: 192,168,1,10)
#define MQTT_HOST       192,168,1,241
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