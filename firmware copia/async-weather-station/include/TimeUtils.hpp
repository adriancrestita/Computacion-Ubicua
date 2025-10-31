#pragma once
#include <Arduino.h>
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
  // Formato: 2025-01-01T00:00:00+01:00 (Ajuste manual del formato %z)
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &timeinfo);
  String ts(buffer);
  // Añade el ':' separador en la zona horaria si falta
  if (ts.length() > 22) ts = ts.substring(0, 22) + ":" + ts.substring(22);
  return ts;
}