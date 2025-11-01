/* ======================================================
 *  Proyecto: Estación Meteorológica Local con WiFi (ESP32)
 *  Autor: [Tu Nombre]
 *  Descripción: Mide variables ambientales y envía datos por MQTT.
 *  ======================================================
 */

#include <WiFi.h>
#include <Wire.h>
#include <AsyncMqttClient.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#include "config/config.h"
#include "include/ESP32_Utils.hpp"
#include "include/ESP32_Utils_MQTT_Async.hpp"
#include "include/MQTT.hpp"
#include "include/TimeUtils.hpp"

// === Definición de pines ===
#define DHTPIN 14
#define DHTTYPE DHT11
#define MQ2_AO 35
#define BUZZER_PIN 15
#define LED_R 12
#define LED_G 13
#define LED_B 27
#define BUTTON_BACK 5
#define BUTTON_NEXT 18
#define ANEMO_PIN 17

// === Configuración del anemómetro ===
constexpr int RecordTime = 3;
volatile int InterruptCounter = 0;
volatile unsigned long lastPulse = 0;
float WindSpeed = 0.0f;
constexpr float ANEMO_FACTOR = 2.4f;

// === Sensores ===
Adafruit_BMP085 bmp;
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

// === Pantalla OLED ===
constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === MQ2 ===
constexpr int MQ2_umbral_base = 1400;
constexpr int MQ2_umbral_malo = 1700;
constexpr int MQ2_umbral_horrible = 1800;

// === MQTT ===
AsyncMqttClient mqttClient;
constexpr unsigned long PUBLISH_INTERVAL_MS = 30UL * 1000UL;

// === Estado del sistema ===
struct SensorData {
  float temperatureC = NAN;
  float humidityPercent = NAN;
  float pressureHpa = NAN;
  float altitudeMeters = NAN;
  float lightLux = NAN;
  float windSpeedKmh = 0.0f;
  float windSpeedMs = 0.0f;
  int gasRaw = 0;
  String gasQuality;
};

SensorData latestSensorData;
bool hasSensorData = false;
String lastReceivedMessage = "Sin mensajes";
volatile bool displayNeedsUpdate = false;
unsigned long lastPublishMillis = 0;
bool mqttConnected = false;

// === Códigos de color ANSI para logs ===
#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_BOLD    "\033[1m"

// =============================================================
// === Función para obtener hora local (TimeUtils) ===
// =============================================================
String getLocalTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String("00:00:00");
  }
  char buffer[16];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

// =============================================================
// === Intro visual para logs ===
// =============================================================
void introLog(const char* titulo, const char* color = ANSI_CYAN) {
  String hora = getLocalTimeString();
  Serial.println();
  Serial.println(String(color) + "======================================================" + ANSI_RESET);
  Serial.printf("[%s] %s%s%s\n", hora.c_str(), color, titulo, ANSI_RESET);
  Serial.println(String(color) + "======================================================" + ANSI_RESET);
}

// === Prototipos ===
String getCalidadAire(int gasADC);
float measureWind();
void IRAM_ATTR countup();
SensorData readSensors();
void logSensorData(const SensorData& data);
String buildSensorPayload(const SensorData& data);
void publishCurrentData();
void updateDisplayIfNeeded();

// =============================================================
// === Interrupción: contar pulsos del anemómetro ===
// =============================================================
void IRAM_ATTR countup() {
  unsigned long now = micros();
  if (now - lastPulse > 5000) {
    InterruptCounter++;
    lastPulse = now;
  }
}

// =============================================================
// === Medición del viento ===
// =============================================================
float measureWind() {
  InterruptCounter = 0;
  attachInterrupt(digitalPinToInterrupt(ANEMO_PIN), countup, FALLING);
  delay(RecordTime * 1000);
  detachInterrupt(digitalPinToInterrupt(ANEMO_PIN));
  WindSpeed = (float)InterruptCounter / (float)RecordTime * ANEMO_FACTOR;
  return WindSpeed;
}

// =============================================================
// === SETUP ===
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  introLog("🌦 Iniciando Estación Meteorológica Local con MQTT...", ANSI_CYAN);

  Wire.begin(21, 22);
  dht.begin();
  lightMeter.begin();

  if (!bmp.begin()) {
    introLog("❌ Error: No se detecta BMP180/BMP085.", ANSI_RED);
    while (true) delay(1000);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    introLog("❌ Error: No se pudo inicializar OLED.", ANSI_RED);
    while (true) delay(1000);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.println("Estación iniciada.");
  display.display();

  pinMode(BUTTON_BACK, INPUT_PULLDOWN);
  pinMode(BUTTON_NEXT, INPUT_PULLDOWN);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(ANEMO_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  WiFi.onEvent(WiFiEvent);
  ConnectWiFi_STA();
  InitMqtt();
  initTime();

  lastPublishMillis = millis() - PUBLISH_INTERVAL_MS;
  displayNeedsUpdate = true;

  introLog("✅ Sistema iniciado correctamente.", ANSI_GREEN);
}

// =============================================================
// === LOOP ===
// =============================================================
void loop() {
  HandleMqttTasks();
  mqttConnected = mqttClient.connected();

  unsigned long now = millis();
  if (now - lastPublishMillis >= PUBLISH_INTERVAL_MS) {
    introLog("📡 Publicando nuevos datos...");
    publishCurrentData();
    lastPublishMillis = now;
  }

  updateDisplayIfNeeded();
  delay(10);
}

// =============================================================
// === Lectura de sensores ===
// =============================================================
SensorData readSensors() {
  introLog("🔍 Lectura de sensores en curso...", ANSI_BLUE);

  SensorData data;
  data.temperatureC = dht.readTemperature();
  data.humidityPercent = dht.readHumidity();

  if (isnan(data.temperatureC) || isnan(data.humidityPercent)) {
    delay(200);
    data.temperatureC = dht.readTemperature();
    data.humidityPercent = dht.readHumidity();
  }

  data.pressureHpa = bmp.readPressure() / 100.0f;
  data.altitudeMeters = bmp.readAltitude();
  data.lightLux = lightMeter.readLightLevel();
  data.gasRaw = analogRead(MQ2_AO);
  data.windSpeedKmh = measureWind();
  data.windSpeedMs = data.windSpeedKmh / 3.6f;
  data.gasQuality = getCalidadAire(data.gasRaw);

  return data;
}

// =============================================================
// === Logs de datos de sensores ===
// =============================================================
void logSensorData(const SensorData& data) {
  introLog("📊 Lecturas obtenidas:", ANSI_CYAN);
  Serial.printf(ANSI_GREEN "🌡 Temp: %.1f °C\n" ANSI_RESET, data.temperatureC);
  Serial.printf(ANSI_GREEN "💧 Humedad: %.1f %%\n" ANSI_RESET, data.humidityPercent);
  Serial.printf(ANSI_GREEN "🧭 Presión: %.1f hPa\n" ANSI_RESET, data.pressureHpa);
  Serial.printf(ANSI_GREEN "⛰ Altitud: %.1f m\n" ANSI_RESET, data.altitudeMeters);
  Serial.printf(ANSI_GREEN "☀️ Luz: %.1f lx\n" ANSI_RESET, data.lightLux);
  Serial.printf(ANSI_GREEN "🌬 Viento: %.1f km/h (%.2f m/s)\n" ANSI_RESET, data.windSpeedKmh, data.windSpeedMs);
  Serial.printf(ANSI_GREEN "🧪 MQ2: %d (%s)\n" ANSI_RESET, data.gasRaw, data.gasQuality.c_str());
  Serial.println(ANSI_CYAN "===================================================\n" ANSI_RESET);
}

// =============================================================
// === Construcción de JSON ===
// =============================================================
String buildSensorPayload(const SensorData& data) {
  introLog("🧱 Construyendo JSON de datos...", ANSI_YELLOW);

  StaticJsonDocument<512> doc;
  doc["sensor_id"] = "WS_001";
  doc["sensor_type"] = "weather";
  doc["street_id"] = "ST_1253";
  doc["timestamp"] = getTimestampISO8601();

  JsonObject location = doc.createNestedObject("location");
  location["latitude"] = 40.4094736;
  location["longitude"] = -3.6920903;
  location["altitude_meters"] = data.altitudeMeters;
  location["district"] = "Centro";
  location["neighborhood"] = "Universidad";

  JsonObject readings = doc.createNestedObject("data");
  readings["temperature_celsius"] = data.temperatureC;
  readings["humidity_percentage"] = data.humidityPercent;
  readings["wind_speed"] = data.windSpeedKmh;
  readings["luz"] = data.lightLux;
  readings["atmospheric_pressure_hpa"] = data.pressureHpa;
  readings["air_quality_index"] = data.gasRaw;

  String json;
  serializeJson(doc, json);
  return json;
}

// =============================================================
// === Publicación de datos MQTT ===
// =============================================================
void publishCurrentData() {
  SensorData data = readSensors();
  latestSensorData = data;
  hasSensorData = true;
  displayNeedsUpdate = true;

  logSensorData(data);

  String payload = buildSensorPayload(data);
  if (!PublishMqtt(payload)) {
    introLog("❌ Error publicando datos MQTT.", ANSI_RED);
  } else {
    introLog("✅ Datos MQTT publicados correctamente.", ANSI_GREEN);
  }
}

// =============================================================
// === Actualización OLED ===
// =============================================================
void updateDisplayIfNeeded() {
  if (!displayNeedsUpdate) return;
  displayNeedsUpdate = false;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  String hora = getLocalTimeString();
  display.printf("== Estacion Local ==\n%s\n", hora.c_str());

  if (!mqttConnected) {
    display.setTextSize(1);
    display.setCursor(0, 24);
    display.println("⚠️ MQTT sin conexión");
    display.display();
    return;
  }

  if (!hasSensorData) {
    display.println("Sin lecturas");
    display.display();
    return;
  }

  display.printf("T: %.1fC H: %.1f%%\n", latestSensorData.temperatureC, latestSensorData.humidityPercent);
  display.printf("Pres: %.1f hPa\n", latestSensorData.pressureHpa);
  display.printf("Alt: %.1f m\n", latestSensorData.altitudeMeters);
  display.printf("Luz: %.1f lx\n", latestSensorData.lightLux);
  display.printf("Viento: %.1f km/h\n", latestSensorData.windSpeedKmh);
  display.printf("Gas: %d (%s)\n", latestSensorData.gasRaw, latestSensorData.gasQuality.c_str());
  display.display();
}

// =============================================================
// === Calidad del aire ===
// =============================================================
String getCalidadAire(int gasADC) {
  if (gasADC > MQ2_umbral_horrible) return "PELIGROSA";
  if (gasADC > MQ2_umbral_malo) return "MALA";
  if (gasADC > MQ2_umbral_base) return "NORMAL";
  return "BUENA";
}

// =============================================================
// === CALLBACK MQTT: Mensajes recibidos ===
// =============================================================
void OnMqttReceived(char* topic,
                    char* payload,
                    AsyncMqttClientMessageProperties properties,
                    size_t len,
                    size_t index,
                    size_t total) {
  static String accumulatedPayload;

  if (index == 0) accumulatedPayload = "";
  accumulatedPayload += String(payload).substring(0, len);

  if (index + len < total) return;

  Serial.println(ANSI_CYAN "========== [MQTT MENSAJE RECIBIDO] ==========" ANSI_RESET);
  Serial.printf(ANSI_BOLD "Topic: %s\n" ANSI_RESET, topic);
  Serial.printf(ANSI_BOLD "Payload: %s\n" ANSI_RESET, accumulatedPayload.c_str());
  Serial.println(ANSI_CYAN "=============================================" ANSI_RESET);

  lastReceivedMessage = accumulatedPayload;
  displayNeedsUpdate = true;
}
