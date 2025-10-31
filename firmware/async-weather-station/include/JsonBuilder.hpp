#pragma once
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

// === CONFIGURACIÓN NTP PARA ZONA HORARIA DE MADRID ===
#define NTP_SERVER "pool.ntp.org"
#define TZ_INFO "CET-1CEST,M3.5.0/2,M10.5.0/3" // España/Madrid

// === Inicializa la sincronización NTP ===
void initTime() {
  configTime(0, 0, NTP_SERVER);
  setenv("TZ", TZ_INFO, 1);
  tzset();

  Serial.println("⏰ Sincronizando hora NTP...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ Hora sincronizada correctamente (zona Madrid).");
}

// === Devuelve timestamp en formato ISO8601 ===
String getTimestampISO8601() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00Z";
  }

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &timeinfo);
  String ts(buffer);
  if (ts.length() > 22) ts = ts.substring(0, 22) + ":" + ts.substring(22);
  return ts;
}

/**
 * @brief Construye un JSON con toda la información de la estación
 * en un único mensaje con la estructura solicitada.
 */
String buildWeatherStationJson(
  const char* sensor_id,
  const char* street_id,
  double latitude,
  double longitude,
  double altitude,
  const char* district,
  const char* neighborhood,
  double temperature_celsius,
  double humidity_percentage,
  double wind_speed_kmh,
  double light_lux,
  double atmospheric_pressure_hpa,
  double air_quality_index
) {
  StaticJsonDocument<768> doc;

  doc["sensor_id"] = sensor_id;
  doc["sensor_type"] = "weather";
  doc["street_id"] = street_id;
  doc["timestamp"] = getTimestampISO8601();

  JsonObject loc = doc.createNestedObject("location");
  loc["latitude"] = latitude;
  loc["longitude"] = longitude;
  loc["altitude_meters"] = altitude;
  loc["district"] = district;
  loc["neighborhood"] = neighborhood;

  JsonObject data = doc.createNestedObject("data");
  data["temperature_celsius"] = temperature_celsius;
  data["humidity_percentage"] = humidity_percentage;
  data["wind_speed"] = wind_speed_kmh;
  data["luz"] = light_lux;
  data["atmospheric_pressure_hpa"] = atmospheric_pressure_hpa;
  data["air_quality_index"] = air_quality_index;

  String json;
  serializeJson(doc, json);
  Serial.println(F("[DEBUG] JSON generado:"));
  Serial.println(json);
  return json;
}

// Función legacy mantenida por compatibilidad con otros proyectos.
String buildSensorJsonFor(
  const char* sensor_id,
  const char* sensor_type,
  const char* street_id,
  double latitude,
  double longitude,
  double altitude,
  const char* data_key,
  double data_value,
  const char* district = "Centro",
  const char* neighborhood = "Universidad"
) {
  StaticJsonDocument<512> doc;

  doc["sensor_id"] = sensor_id;
  doc["sensor_type"] = sensor_type;
  doc["street_id"] = street_id;
  doc["timestamp"] = getTimestampISO8601();

  JsonObject loc = doc.createNestedObject("location");
  loc["latitude"] = latitude;
  loc["longitude"] = longitude;
  loc["altitude_meters"] = altitude;
  loc["district"] = district;
  loc["neighborhood"] = neighborhood;

  JsonObject data = doc.createNestedObject("data");
  data[data_key] = data_value;

  String json;
  serializeJson(doc, json);
  Serial.println(F("[DEBUG] JSON generado:"));
  Serial.println(json);
  return json;
}
