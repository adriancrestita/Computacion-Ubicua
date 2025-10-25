#pragma once
#include <WiFi.h>
#include <time.h>

/**
 * @brief Inicializa la sincronización NTP para la zona horaria de Madrid (España).
 * 
 * Gestiona automáticamente los cambios entre horario de invierno (CET)
 * y de verano (CEST).
 * 
 * @param ntpServer Servidor NTP a usar (por defecto "pool.ntp.org")
 */
void initNTP(const char* ntpServer = "pool.ntp.org") {
  Serial.println("⏰ Iniciando sincronización NTP (zona horaria: Madrid)");

  // Zona horaria de España (Europa/Madrid)
  // CET-1CEST,M3.5.0,M10.5.0/3 → UTC+1, cambio horario último domingo de marzo y octubre
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", ntpServer);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("⚠️  Error al obtener tiempo NTP.");
    return;
  }

  Serial.printf("✅ Hora NTP sincronizada: %02d:%02d:%02d (%02d/%02d/%04d)\n",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
}

/**
 * @brief Devuelve la hora actual en formato ISO-8601 UTC (ej. "2025-10-17T12:34:56.000Z").
 * 
 * Aunque el sistema trabaja internamente en horario local (Madrid),
 * este formato devuelve la hora en UTC (con sufijo "Z").
 * 
 * @return String con timestamp UTC.
 */
String getTimestampISO8601() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00.000Z"; // fallback si no hay hora válida
  }

  // Obtenemos el tiempo en formato UTC
  time_t now;
  time(&now);
  gmtime_r(&now, &timeinfo);  // Convertir a UTC real

  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
  return String(timestamp);
}