#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <math.h>

// ====== MQTT/WiFi (tus utilidades) ======
#include "config.h"
#include "ESP32_Utils.hpp"
#include "ESP32_Utils_MQTT_Async.hpp"
#include "MQTT.hpp"

// ====== JSON + NTP (Madrid) ======
#include "JsonBuilder.hpp"

// === PINES DEFINIDOS ===
#define DHTPIN 14
#define DHTTYPE DHT11
#define MQ2_DO 25
#define MQ2_AO 35
#define BUZZER_PIN 15
#define LED_R 12
#define LED_G 13
#define LED_B 27
#define BUTTON_BACK 5
#define BUTTON_NEXT 18

// === ANEMÓMETRO (interrupción) ===
const int SensorPin = 17;       // GPIO 17: interrupción
const int RecordTime = 3;       // Tiempo de medición (s)
volatile int InterruptCounter = 0;
float WindSpeed = 0.0;

// === OBJETOS DE SENSORES ===
Adafruit_BMP085 bmp;
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

// === PANTALLA OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === VARIABLES INTERFAZ ===
int menuIndex = 5;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 300;
bool lastButtonBack = LOW;
bool lastButtonNext = LOW;

// === INTERVALO DE PUBLICACIÓN MQTT (MODIFICABLE) ===
const unsigned long DEFAULT_MQTT_INTERVAL_MS = 60UL * 1000UL;  // 1 minuto
unsigned long mqttPublishIntervalMs = DEFAULT_MQTT_INTERVAL_MS;
unsigned long lastMqttPublish = 0;

const char* WEATHER_SENSOR_ID = "WS_001";
const char* WEATHER_STREET_ID = "ST_1253";
const double WEATHER_LATITUDE = 40.4094736;
const double WEATHER_LONGITUDE = -3.6920903;
const char* WEATHER_DISTRICT = "Centro";
const char* WEATHER_NEIGHBORHOOD = "Universidad";

// === VARIABLES MQTT RECIBIDAS ===
float mqttTemp = NAN;
float mqttHum = NAN;
float mqttPres = NAN;
float mqttLuz = NAN;
float mqttWindSpeed = NAN;
float mqttAltitude = NAN;
String mqttAirQuality;
String mqttTimestamp;
unsigned long lastMqttDataUpdate = 0;

// === PROTOTIPOS ===
void handleAlert(int estado);
void showDHT_aux_MQTT();
void showBMP_MQTT();
void showBH1750_MQTT();
void showAnemo_aux_MQTT();
void showMQ2_aux_MQTT();
void showResumen_MQTT();
bool updateMqttDataFromPayload(const String& payload);
bool hasFreshMqttData();
String formatTimestampForDisplay(const String& isoTimestamp);
float calcularAltitud(float P_medida, float P_referencia);
String getCalidadAire(int gasADC);

// === ANEMÓMETRO ===
void IRAM_ATTR countup() { InterruptCounter++; }

void measureWind() {
  InterruptCounter = 0;
  attachInterrupt(SensorPin, countup, RISING);
  delay(RecordTime * 1000);
  detachInterrupt(SensorPin);
  WindSpeed = (float)InterruptCounter / (float)RecordTime * 2.4;  // km/h
}

// === MQTT CALLBACK ===
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  String topicStr(topic);

  if (topicStr != "estacion/weather") {
    Serial.printf("📩 MQTT ignorado [%s]: %s\n", topic, msg.c_str());
    return;
  }

  if (updateMqttDataFromPayload(msg)) {
    Serial.printf("📩 MQTT [%s]: %s\n", topic, msg.c_str());
  } else {
    Serial.printf("⚠️ Error al procesar MQTT [%s]: %s\n", topic, msg.c_str());
  }
}

// === PUBLICAR MQTT ===
void publicarSensor(const char* topic, const String& payload) {
  PublishMqttMessage(topic, payload, true);
  Serial.printf("\n📡 Publicado en [%s]: %s\n", topic, payload.c_str());
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  dht.begin();
  lightMeter.begin();
  bmp.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Error OLED");
    while (1);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  pinMode(MQ2_DO, INPUT);
  pinMode(BUTTON_BACK, INPUT_PULLDOWN);
  pinMode(BUTTON_NEXT, INPUT_PULLDOWN);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(SensorPin, INPUT_PULLUP);

  Serial.println("Sistema iniciado correctamente.");

  // WiFi + MQTT
  ConnectWiFi_STA(true);
  InitMqtt();
  mqttClient.setCallback(onMqttMessage);
  EnsureMqttConnection();

  // Suscribirse al topic unificado
  mqttClient.subscribe("estacion/weather");
  initTime();
}

// === LOOP ===
void loop() {
  HandleMqttLoop();

  unsigned long now = millis();

  // --- BOTONES ---
  bool backState = digitalRead(BUTTON_BACK);
  bool nextState = digitalRead(BUTTON_NEXT);
  if (backState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    menuIndex = (menuIndex - 1 + 6) % 6;
    lastDebounceTime = millis();
  }
  if (nextState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    menuIndex = (menuIndex + 1) % 6;
    lastDebounceTime = millis();
  }
  lastButtonBack = backState;
  lastButtonNext = nextState;

  // --- LECTURA LOCAL + PUBLICACIÓN MQTT ---
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  float pres = bmp.readPressure() / 100.0;
  float lux = lightMeter.readLightLevel();
  int gasA = analogRead(MQ2_AO);
  float altitud = calcularAltitud(pres, 1013.25);

  if (now - lastMqttPublish >= mqttPublishIntervalMs) {
    lastMqttPublish = now;
    measureWind();
    float windSpeed = WindSpeed;
    String calidadAire = getCalidadAire(gasA);

    String payload = buildWeatherStationJson(
      WEATHER_SENSOR_ID,
      WEATHER_STREET_ID,
      WEATHER_LATITUDE,
      WEATHER_LONGITUDE,
      altitud,
      WEATHER_DISTRICT,
      WEATHER_NEIGHBORHOOD,
      temp,
      hum,
      windSpeed,
      lux,
      pres,
      calidadAire
    );

    publicarSensor("estacion/weather", payload);

    // Reutiliza el mismo flujo de parseo que los mensajes entrantes.
    if (!updateMqttDataFromPayload(payload)) {
      Serial.println("⚠️ No se pudo actualizar el estado local con el JSON enviado.");
    }
  }

  // --- PANTALLA OLED ---
  display.clearDisplay();
  display.setCursor(0, 0);
  switch (menuIndex) {
    case 0: showDHT_aux_MQTT(); break;
    case 1: showBMP_MQTT(); break;
    case 2: showBH1750_MQTT(); break;
    case 3: showAnemo_aux_MQTT(); break;
    case 4: showMQ2_aux_MQTT(); break;
    case 5: showResumen_MQTT(); break;
  }
  display.display();
}

// === FUNCIONES AUXILIARES ===
float calcularAltitud(float P_medida, float P_referencia) {
  return 44330.0f * (1.0f - pow((P_medida / P_referencia), 0.19029f));
}

String getCalidadAire(int gasADC) {
  if (gasADC > 1800) return "PELIGROSA";
  else if (gasADC > 1700) return "MALA";
  else if (gasADC > 1400) return "NORMAL";
  return "BUENA";
}

// === PANTALLAS MQTT ===
void showDHT_aux_MQTT() {
  display.setTextSize(2); display.println("DHT11");
  display.setTextSize(1);
  if (isnan(mqttTemp) || isnan(mqttHum)) display.println("Sin datos MQTT");
  else {
    display.printf("Temp: %.1f C\n", mqttTemp);
    display.printf("Hum : %.1f %%", mqttHum);
  }
}

void showBMP_MQTT() {
  display.setTextSize(2); display.println("BMP180");
  display.setTextSize(1);
  if (isnan(mqttPres)) display.println("Sin datos MQTT");
  else {
    display.printf("Pres: %.1f hPa\n", mqttPres);
    if (!isnan(mqttAltitude)) display.printf("Alt : %.1f m", mqttAltitude);
  }
}

void showBH1750_MQTT() {
  display.setTextSize(2); display.println("BH1750");
  display.setTextSize(1);
  if (isnan(mqttLuz)) display.println("Sin datos MQTT");
  else display.printf("Luz: %.1f lx", mqttLuz);
}

void showAnemo_aux_MQTT() {
  display.setTextSize(2); display.println("Viento");
  display.setTextSize(1);
  if (isnan(mqttWindSpeed)) display.println("Sin datos MQTT");
  else {
    display.printf("MQTT: %.1f km/h\n", mqttWindSpeed);
    display.printf("m/s: %.2f", mqttWindSpeed / 3.6);
  }
}

void showMQ2_aux_MQTT() {
  display.setTextSize(2); display.println("MQ2");
  display.setTextSize(1);
  if (mqttAirQuality.isEmpty()) display.println("Sin datos MQTT");
  else {
    display.println("Calidad aire:");
    display.println(mqttAirQuality);
  }
}

void showResumen_MQTT() {
  display.setTextSize(1);
  display.println("RESUMEN MQTT");
  display.println("--------------------");
  if (!hasFreshMqttData()) {
    display.println("Esperando datos\nMQTT...");
    return;
  }

  display.printf("Temp: %.1f C\n", mqttTemp);
  display.printf("Hum : %.1f %%\n", mqttHum);
  display.printf("Pres: %.1f hPa\n", mqttPres);
  display.printf("Luz : %.1f lx\n", mqttLuz);
  display.printf("Viento: %.1f km/h\n", mqttWindSpeed);
  if (mqttAirQuality.isEmpty()) display.println("Gas: N/D");
  else display.printf("Gas: %s\n", mqttAirQuality.c_str());
  if (!mqttTimestamp.isEmpty()) {
    display.println("--------------------");
    display.println("Ult. MQTT:");
    display.println(formatTimestampForDisplay(mqttTimestamp));
  }
  unsigned long ageSeconds = (millis() - lastMqttDataUpdate) / 1000UL;
  display.println("--------------------");
  display.printf("Hace: %lus\n", ageSeconds);
}

bool updateMqttDataFromPayload(const String& payload) {
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("deserializeJson falló: %s\n", err.c_str());
    return false;
  }

  JsonVariantConst data = doc["data"];
  if (data.isNull()) return false;

  mqttTemp = data["temperature_celsius"] | NAN;
  mqttHum = data["humidity_percentage"] | NAN;
  mqttPres = data["atmospheric_pressure_hpa"] | NAN;
  mqttLuz = data["luz"] | NAN;
  mqttWindSpeed = data["wind_speed"] | NAN;
  const char* air = data["air_quality_index"] | "";
  mqttAirQuality = String(air);

  JsonVariantConst location = doc["location"];
  mqttAltitude = location["altitude_meters"] | NAN;

  const char* ts = doc["timestamp"] | "";
  mqttTimestamp = String(ts);
  lastMqttDataUpdate = millis();
  return true;
}

bool hasFreshMqttData() {
  return lastMqttDataUpdate != 0;
}

String formatTimestampForDisplay(const String& isoTimestamp) {
  if (isoTimestamp.length() < 19) return isoTimestamp;
  String datePart = isoTimestamp.substring(0, 10);
  String timePart = isoTimestamp.substring(11, 19);
  return datePart + " " + timePart;
}
