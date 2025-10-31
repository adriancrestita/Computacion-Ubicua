/* ======================================================
 *  Proyecto: Estación Meteorológica Local con WiFi (ESP32)
 *  Descripción: Mide temperatura, humedad, presión, luz,
 *  velocidad del viento y calidad del aire.
 *  Muestra los datos en OLED, consola serial y los envía
 *  por MQTT cada 30 segundos.
 *  ======================================================
 */

#include "../../config/config.h"

#include <WiFi.h>
#include <Wire.h>
#include <AsyncMqttClient.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#include "../../ESP32_Utils.hpp"
#include "../../ESP32_Utils_MQTT_Async.hpp"
#include "../../MQTT.hpp"
#include "../../TimeUtils.hpp"

// === Pines ===
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
constexpr int RecordTime = 3;                // Tiempo de muestreo (s)
volatile int InterruptCounter = 0;           // Contador de interrupciones
volatile unsigned long lastPulse = 0;        // Tiempo del último pulso (µs)
float WindSpeed = 0.0f;                      // Velocidad del viento en km/h
constexpr float ANEMO_FACTOR = 2.4f;         // Factor de conversión a km/h

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

// === Estado de aplicación ===
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

// === Prototipos ===
void renderDisplay(void (*pageFunc)());
void showResumen();
String getCalidadAire(int gasADC);
float measureWind();
void IRAM_ATTR countup();
SensorData readSensors();
void logSensorData(const SensorData& data);
String buildSensorPayload(const SensorData& data);
void publishCurrentData();
void updateDisplayIfNeeded();

// Implementado al final del archivo (requerido por AsyncMqttClient)
void OnMqttReceived(char* topic,
                    char* payload,
                    AsyncMqttClientMessageProperties properties,
                    size_t len,
                    size_t index,
                    size_t total);

// =============================================================
// === INTERRUPCIÓN: contar pulsos del anemómetro ===
// =============================================================
void IRAM_ATTR countup() {
  unsigned long now = micros();
  if (now - lastPulse > 5000) {  // antirrebote: ignora pulsos <5 ms
    InterruptCounter++;
    lastPulse = now;
  }
}

// =============================================================
// === CÁLCULO DE VELOCIDAD DEL VIENTO ===
// =============================================================
float measureWind() {
  InterruptCounter = 0;
  attachInterrupt(digitalPinToInterrupt(ANEMO_PIN), countup, FALLING);
  delay(RecordTime * 1000);
  detachInterrupt(digitalPinToInterrupt(ANEMO_PIN));
  WindSpeed = static_cast<float>(InterruptCounter) / static_cast<float>(RecordTime) * ANEMO_FACTOR;
  return WindSpeed;
}

// =============================================================
// === SETUP ===
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("🌦 Iniciando Estación Meteorológica Local con MQTT...");

  Wire.begin(21, 22);

  // --- Sensores ---
  dht.begin();
  lightMeter.begin();

  if (!bmp.begin()) {
    Serial.println("❌ No se detecta BMP180/BMP085. Revisa conexiones.");
    while (true) {
      delay(1000);
    }
  }

  // --- OLED ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ No se pudo inicializar OLED.");
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.println("Estacion con MQTT OK!");
  display.display();

  // --- Pines ---
  pinMode(BUTTON_BACK, INPUT_PULLDOWN);
  pinMode(BUTTON_NEXT, INPUT_PULLDOWN);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(ANEMO_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // --- WiFi + MQTT ---
  WiFi.onEvent(WiFiEvent);
  ConnectWiFi_STA();
  InitMqtt();
  ConnectToMqtt();

  initTime();

  lastPublishMillis = millis() - PUBLISH_INTERVAL_MS;  // Fuerza el primer envío inmediato
  displayNeedsUpdate = true;

  Serial.println("✅ Sistema iniciado correctamente.");
}

// =============================================================
// === LOOP PRINCIPAL ===
// =============================================================
void loop() {
  unsigned long now = millis();

  if (now - lastPublishMillis >= PUBLISH_INTERVAL_MS) {
    publishCurrentData();
    lastPublishMillis = now;
  }

  updateDisplayIfNeeded();
  delay(10);
}

// =============================================================
// === FUNCIONES AUXILIARES ===
// =============================================================
SensorData readSensors() {
  SensorData data;

  data.temperatureC = dht.readTemperature();
  data.humidityPercent = dht.readHumidity();

  // Reintenta una vez si hay lectura inválida del DHT
  if (isnan(data.temperatureC) || isnan(data.humidityPercent)) {
    delay(200);
    data.temperatureC = dht.readTemperature();
    data.humidityPercent = dht.readHumidity();
  }

  data.pressureHpa = bmp.readPressure() / 100.0f;  // hPa
  data.altitudeMeters = bmp.readAltitude();         // metros
  data.lightLux = lightMeter.readLightLevel();      // lux
  data.gasRaw = analogRead(MQ2_AO);                 // MQ2 analógico

  data.windSpeedKmh = measureWind();
  data.windSpeedMs = data.windSpeedKmh / 3.6f;
  data.gasQuality = getCalidadAire(data.gasRaw);

  return data;
}

void logSensorData(const SensorData& data) {
  Serial.println("========== [SENSORES] ==========");
  Serial.printf("🌡 Temp: %.1f °C\n", data.temperatureC);
  Serial.printf("💧 Humedad: %.1f %%\n", data.humidityPercent);
  Serial.printf("🧭 Presion: %.1f hPa\n", data.pressureHpa);
  Serial.printf("⛰ Altitud: %.1f m\n", data.altitudeMeters);
  Serial.printf("☀️  Luz: %.1f lx\n", data.lightLux);
  Serial.printf("🌬 Viento: %.1f km/h (%.2f m/s)\n", data.windSpeedKmh, data.windSpeedMs);
  Serial.printf("🧪 MQ2: %d (%s)\n", data.gasRaw, data.gasQuality.c_str());
  Serial.println("=================================");
}

String buildSensorPayload(const SensorData& data) {
  StaticJsonDocument<512> doc;

  doc["sensor_id"] = "WT_001";
  doc["street_id"] = "street_1253";
  doc["timestamp"] = getTimestampISO8601();

  JsonObject readings = doc.createNestedObject("data");
  readings["temperature_c"] = data.temperatureC;
  readings["humidity_percent"] = data.humidityPercent;
  readings["pressure_hpa"] = data.pressureHpa;
  readings["altitude_m"] = data.altitudeMeters;
  readings["light_lux"] = data.lightLux;
  readings["wind_speed_kmh"] = data.windSpeedKmh;
  readings["wind_speed_ms"] = data.windSpeedMs;
  readings["gas_adc"] = data.gasRaw;
  readings["gas_quality"] = data.gasQuality;

  JsonObject meta = doc.createNestedObject("meta");
  meta["mqtt_topic"] = MQTT_TOPIC;
  meta["device_ip"] = WiFi.localIP().toString();

  String json;
  serializeJson(doc, json);
  return json;
}

void publishCurrentData() {
  SensorData data = readSensors();
  latestSensorData = data;
  hasSensorData = true;
  displayNeedsUpdate = true;

  logSensorData(data);

  String payload = buildSensorPayload(data);
  if (!PublishMqtt(payload)) {
    // Advertencia: el mensaje no se pudo publicar porque no hay sesión MQTT activa.
    Serial.println("[WARN] No se pudo publicar el payload MQTT.");
  }
}

void updateDisplayIfNeeded() {
  if (!displayNeedsUpdate) {
    return;
  }

  displayNeedsUpdate = false;
  renderDisplay(showResumen);
}

// =============================================================
// === VISUALIZACIÓN OLED ===
// =============================================================
void showResumen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("== Estacion Local ==");

  if (!hasSensorData) {
    display.println("Sin lecturas");
    display.display();
    return;
  }

  display.printf("T: %.1f C  H: %.1f%%\n", latestSensorData.temperatureC, latestSensorData.humidityPercent);
  display.printf("Pres: %.1f hPa\n", latestSensorData.pressureHpa);
  display.printf("Alt: %.1f m\n", latestSensorData.altitudeMeters);
  display.printf("Luz: %.1f lx\n", latestSensorData.lightLux);
  display.printf("Viento: %.1f km/h (%.2f m/s)\n", latestSensorData.windSpeedKmh, latestSensorData.windSpeedMs);
  display.printf("Gas: %d (%s)\n", latestSensorData.gasRaw, latestSensorData.gasQuality.c_str());

  display.setCursor(0, 48);
  display.println("MQTT msg:");
  display.setTextSize(1);

  String line1 = lastReceivedMessage;
  line1.replace('\n', ' ');
  line1.replace('\r', ' ');
  if (line1.length() > 21) {
    display.println(line1.substring(0, 21));
    if (line1.length() > 42) {
      display.println(line1.substring(21, 42));
    } else {
      display.println(line1.substring(21));
    }
  } else {
    display.println(line1);
  }

  display.display();
}

void renderDisplay(void (*pageFunc)()) {
  pageFunc();
}

// =============================================================
// === CALIDAD DEL AIRE MQ2 ===
// =============================================================
String getCalidadAire(int gasADC) {
  if (gasADC > MQ2_umbral_horrible) return "PELIGROSA";
  if (gasADC > MQ2_umbral_malo) return "MALA";
  if (gasADC > MQ2_umbral_base) return "NORMAL";
  return "BUENA";
}

// =============================================================
// === CALLBACKS MQTT ===
// =============================================================
void OnMqttReceived(char* topic,
                    char* payload,
                    AsyncMqttClientMessageProperties properties,
                    size_t len,
                    size_t index,
                    size_t total) {
  static String accumulatedPayload;

  if (index == 0) {
    accumulatedPayload = "";
  }

  accumulatedPayload += GetPayloadContent(payload, len);

  if (index + len < total) {
    return;  // Espera al último fragmento
  }

  lastReceivedMessage = accumulatedPayload;
  displayNeedsUpdate = true;

  Serial.println("---------- [MQTT RECIBIDO] ----------");
  Serial.printf("Topic: %s\n", topic);
  Serial.printf("Payload: %s\n", accumulatedPayload.c_str());
  Serial.println("-------------------------------------");
}
