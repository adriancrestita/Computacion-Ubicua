#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "TimeUtils.hpp"

/**
 * @brief Construye un JSON con toda la información de la estación
 * en un único mensaje con la estructura solicitada.
 */
String buildWeatherStationJson(
  const char* sensor_id,
  const char* sensor_type,
  const char* street_id,
  const char* district,
  const char* neighborhood,
  double latitude,
  double longitude,
  double altitude,
  double temperature_celsius,
  double humidity_percentage,
  double wind_speed,
  double light_lux,
  double atmospheric_pressure_hpa,
  const String& air_quality_index
) {
  StaticJsonDocument<768> doc;

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
  data["temperature_celsius"] = temperature_celsius;
  data["humidity_percentage"] = humidity_percentage;
  data["wind_speed"] = wind_speed;
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
