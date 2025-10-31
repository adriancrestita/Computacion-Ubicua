/* ======================================================
 *  Proyecto: Estación Meteorológica con ESP32
 *  Descripción: Mide temperatura, humedad, presión, luz, 
 *  velocidad del viento y calidad del aire. Publica datos
 *  a través de MQTT y los muestra en pantalla OLED.
 *  Autor: [Tu Nombre]
 *  Fecha: [Actualizar Fecha Actual]
 *  ======================================================
 */

#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>

#include <WiFi.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <AsyncMqttClient.h>

#include "../../config/config.h"
#include "../../include/ESP32_Utils.hpp"
#include "../../include/ESP32_Utils_MQTT_Async.hpp"
#include "../../include/MQTT.hpp"
#include "../../include/JsonBuilder.hpp"

// Cliente MQTT principal
AsyncMqttClient mqttClient;

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
const int RecordTime = 3;              // Tiempo de muestreo (segundos)
volatile int InterruptCounter = 0;     // Contador de interrupciones de viento
float WindSpeed = 0.0;                 // Velocidad del viento en km/h
const float ANEMO_FACTOR = 2.4;        // Factor de conversión a km/h

// === Sensores conectados ===
Adafruit_BMP085 bmp;                   // Sensor de presión BMP180/BMP085
BH1750 lightMeter;                     // Sensor de luz BH1750
DHT dht(DHTPIN, DHTTYPE);              // Sensor de temperatura/humedad DHT11

// === Configuración de pantalla OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === Variables de control de interfaz ===
int menuIndex = 5;                     // Índice actual del menú (por defecto resumen)
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 300;  // Antirrebote botones

// === Configuración MQTT ===
const unsigned long DEFAULT_MQTT_INTERVAL_MS = 60UL * 1000UL;  // 1 minuto
unsigned long mqttPublishIntervalMs = DEFAULT_MQTT_INTERVAL_MS;
unsigned long lastMqttPublish = 0;

// === Datos de estación ===
const char* WEATHER_TOPIC = MQTT_BASE_TOPIC "sensors/weather";
const char* WEATHER_SENSOR_ID = "WS_001";
const char* WEATHER_STREET_ID = "ST_1253";
const double WEATHER_LATITUDE = 40.4094736;
const double WEATHER_LONGITUDE = -3.6920903;
const char* WEATHER_DISTRICT = "Centro";
const char* WEATHER_NEIGHBORHOOD = "Universidad";

// === Variables MQTT recibidas ===
float mqttTemp = NAN, mqttHum = NAN, mqttPres = NAN, mqttLuz = NAN, mqttWindSpeed = NAN, mqttAirQualityIndex = NAN, mqttAltitude = NAN;
String mqttTimestamp;
unsigned long lastMqttDataUpdate = 0;

// === Umbrales de calidad de aire ===
const int MQ2_umbral_base = 1400;
const int MQ2_umbral_malo = 1700;
const int MQ2_umbral_horrible = 1800;

// === Prototipos de funciones ===
void handleAlert(int estado);
void showDHT_aux_MQTT();
void showBMP_MQTT();
void showBH1750_MQTT();
void showAnemo_aux_MQTT();
void showMQ2_aux_MQTT();
void showResumen_MQTT();
String getCalidadAire(int gasADC);
void publicarSensor(const char* topic, const String& payload);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
bool updateMqttDataFromPayload(const String& payload);
bool hasFreshMqttData(unsigned long maxAgeMs = 120000UL);
String formatTimestampForDisplay(const String& isoTimestamp);
void showSensorHeader(const char* title, bool hasData);
void setLED(bool r, bool g, bool b);
void renderDisplay(void (*pageFunc)());

// =============================================================
// === Lógica del anemómetro ===
// =============================================================

// Interrupción para contar pulsos del sensor de viento
void IRAM_ATTR countup() { InterruptCounter++; }

// Calcula velocidad del viento promediando durante RecordTime segundos
void measureWind() {
  InterruptCounter = 0;
  attachInterrupt(digitalPinToInterrupt(ANEMO_PIN), countup, RISING);
  delay(RecordTime * 1000);
  detachInterrupt(ANEMO_PIN);
  WindSpeed = (float)InterruptCounter / (float)RecordTime * ANEMO_FACTOR;
}

// =============================================================
// === SETUP: Inicialización de hardware y conexión ===
// =============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando sistema...");
  Wire.begin(21, 22);

  // Inicialización de sensores
  dht.begin();
  lightMeter.begin();
  bmp.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Configuración de pines
  pinMode(BUTTON_BACK, INPUT_PULLDOWN);
  pinMode(BUTTON_NEXT, INPUT_PULLDOWN);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(ANEMO_PIN, INPUT_PULLUP);

  Serial.println("Sistema iniciado correctamente.");

  // Inicialización WiFi + MQTT
  WiFi.onEvent(WiFiEvent);
  InitMqtt();
  mqttClient.onMessage(onMqttMessage);
  mqttClient.subscribe(WEATHER_TOPIC, 0);
  ConnectWiFi_STA(true);
  initTime();

  // Mostrar pantalla inicial de resumen
  renderDisplay(showResumen_MQTT);
}

// =============================================================
// === LOOP PRINCIPAL ===
// =============================================================
void loop() {
  unsigned long now = millis();

  // Lectura de botones con antirrebote
  bool backState = digitalRead(BUTTON_BACK);
  bool nextState = digitalRead(BUTTON_NEXT);

  if (backState == HIGH && (now - lastDebounceTime) > debounceDelay) {
    menuIndex = (menuIndex - 1 + 6) % 6;
    lastDebounceTime = now;
  }
  if (nextState == HIGH && (now - lastDebounceTime) > debounceDelay) {
    menuIndex = (menuIndex + 1) % 6;
    lastDebounceTime = now;
  }

  // Publicar datos cada cierto intervalo
  if (now - lastMqttPublish >= mqttPublishIntervalMs) {
    lastMqttPublish = now;

    // Lecturas de sensores
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    float pres = bmp.readPressure() / 100.0;
    float lux = lightMeter.readLightLevel();
    int gasA = analogRead(MQ2_AO);
    measureWind();
    float windSpeed = WindSpeed;
    float altitud = bmp.readAltitude(1013.25);

    // Construcción del JSON y envío MQTT
    String payload = buildWeatherStationJson(
      WEATHER_SENSOR_ID, WEATHER_STREET_ID, WEATHER_LATITUDE, WEATHER_LONGITUDE,
      altitud, WEATHER_DISTRICT, WEATHER_NEIGHBORHOOD,
      temp, hum, windSpeed, lux, pres, static_cast<double>(gasA)
    );
    Serial.println(payload);
    //publicarSensor(WEATHER_TOPIC, payload);
    
  }

  // Render de la pantalla OLED según menú seleccionado
  switch (menuIndex) {
    case 0: renderDisplay(showDHT_aux_MQTT); break;
    case 1: renderDisplay(showBMP_MQTT); break;
    case 2: renderDisplay(showBH1750_MQTT); break;
    case 3: renderDisplay(showAnemo_aux_MQTT); break;
    case 4: renderDisplay(showMQ2_aux_MQTT); break;
    case 5: renderDisplay(showResumen_MQTT); break;
  }
}

// =============================================================
// === Funciones de visualización OLED ===
// =============================================================

// Dibuja cabecera estándar para cada sensor
void showSensorHeader(const char* title, bool hasData) {
  display.setTextSize(2);
  display.println(title);
  display.setTextSize(1);
  if (!hasData) display.println("Sin datos MQTT");
}

// Muestra datos del sensor DHT11 (temperatura y humedad)
void showDHT_aux_MQTT() {
  bool hasData = !(isnan(mqttTemp) || isnan(mqttHum));
  showSensorHeader("DHT11 (MQTT)", hasData);
  if (hasData) {
    display.printf("Temp: %.1f C\n", mqttTemp);
    display.printf("Hum : %.1f %%", mqttHum);
  }
}

// Muestra presión y altitud BMP180
void showBMP_MQTT() {
  bool hasData = !isnan(mqttPres);
  showSensorHeader("BMP180 (MQTT)", hasData);
  if (hasData) {
    display.printf("Pres: %.1f hPa\n", mqttPres);
    if (!isnan(mqttAltitude)) display.printf("Alt: %.0f m", mqttAltitude);
  }
}

// Muestra intensidad de luz BH1750
void showBH1750_MQTT() {
  bool hasData = !isnan(mqttLuz);
  showSensorHeader("BH1750 (MQTT)", hasData);
  if (hasData) display.printf("Luz: %.1f lx", mqttLuz);
}

// Muestra velocidad del viento
void showAnemo_aux_MQTT() {
  bool hasData = !isnan(mqttWindSpeed);
  showSensorHeader("Viento (MQTT)", hasData);
  if (hasData) {
    display.printf("km/h: %.1f\n", mqttWindSpeed);
    display.printf("m/s: %.2f", mqttWindSpeed / 3.6);
  }
}

// Muestra calidad del aire (sensor MQ2)
void showMQ2_aux_MQTT() {
  bool hasData = !isnan(mqttAirQualityIndex);
  showSensorHeader("MQ2 (MQTT)", hasData);
  if (hasData) {
    display.printf("Indice: %.0f\n", mqttAirQualityIndex);
    display.printf("Calidad: %s", getCalidadAire((int)mqttAirQualityIndex).c_str());
  }
}

// Muestra resumen general de todos los sensores
void showResumen_MQTT() {
  display.setTextSize(1);
  display.println("RESUMEN MQTT (5/5)");
  display.println("--------------------");
  if (!hasFreshMqttData()) {
    display.println("Esperando datos MQTT...");
    return;
  }

  display.printf("T: %.1f C | H: %.1f %%\n", mqttTemp, mqttHum);
  display.printf("Pres: %.1f hPa\n", mqttPres);
  display.printf("Luz : %.1f lx\n", mqttLuz);
  display.printf("Viento: %.1f km/h\n", mqttWindSpeed);
  if (isnan(mqttAirQualityIndex)) display.println("Gas: N/D");
  else display.printf("Gas: %.0f (%s)\n", mqttAirQualityIndex, getCalidadAire((int)mqttAirQualityIndex).c_str());
  if (!isnan(mqttAltitude)) display.printf("Alt: %.0f m\n", mqttAltitude);

  display.println("--------------------");
  if (!mqttTimestamp.isEmpty()) {
    display.println("Ult. MQTT:");
    display.println(formatTimestampForDisplay(mqttTimestamp));
  }
  display.printf("Hace: %lus\n", (millis() - lastMqttDataUpdate) / 1000UL);
}

// =============================================================
// === Comunicación MQTT ===
// =============================================================

// Callback cuando se recibe un mensaje MQTT
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties, size_t len, size_t, size_t) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  if (String(topic) != WEATHER_TOPIC) return;
  updateMqttDataFromPayload(msg);
}

// Publica mensaje en el tópico MQTT
void publicarSensor(const char* topic, const String& payload) {
  mqttClient.publish(topic, 0, true, payload.c_str());
}

// Actualiza variables locales con datos recibidos por MQTT
bool updateMqttDataFromPayload(const String& payload) {
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, payload)) return false;
  JsonVariantConst data = doc["data"];
  if (data.isNull()) return false;

  mqttTemp = data["temperature_celsius"] | NAN;
  mqttHum = data["humidity_percentage"] | NAN;
  mqttPres = data["atmospheric_pressure_hpa"] | NAN;
  mqttLuz = data["luz"] | NAN;
  mqttWindSpeed = data["wind_speed"] | NAN;
  mqttAirQualityIndex = data["air_quality_index"] | NAN;
  mqttAltitude = doc["location"]["altitude_meters"] | NAN;
  mqttTimestamp = String(doc["timestamp"] | "");
  lastMqttDataUpdate = millis();
  return true;
}

// =============================================================
// === Funciones auxiliares ===
// =============================================================

// Devuelve descripción textual de la calidad del aire según valor ADC
String getCalidadAire(int gasADC) {
  if (gasADC > MQ2_umbral_horrible) return "PELIGROSA";
  if (gasADC > MQ2_umbral_malo) return "MALA";
  if (gasADC > MQ2_umbral_base) return "NORMAL";
  return "BUENA";
}

// Indica si los datos MQTT siguen siendo recientes
bool hasFreshMqttData(unsigned long maxAgeMs) {
  return (lastMqttDataUpdate != 0 && millis() - lastMqttDataUpdate < maxAgeMs);
}

// Convierte timestamp ISO a formato legible
String formatTimestampForDisplay(const String& isoTimestamp) {
  if (isoTimestamp.length() < 19) return isoTimestamp;
  return isoTimestamp.substring(0, 10) + " " + isoTimestamp.substring(11, 19);
}

// Control simplificado de LEDs RGB
void setLED(bool r, bool g, bool b) {
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
  digitalWrite(LED_B, b);
}

// Limpia pantalla, ejecuta función de render y actualiza
void renderDisplay(void (*pageFunc)()) {
  display.clearDisplay();
  display.setCursor(0, 0);
  pageFunc();
  display.display();
}

// Controla indicadores visuales y sonoros según nivel de alerta
void handleAlert(int estado) {
  if (estado == 0) {    // OK
    setLED(false, true, false);
    digitalWrite(BUZZER_PIN, LOW);
  } else if (estado == 1) {   // PRE-ALERTA
    setLED(true, true, false);
    if ((millis() / 1000) % 20 == 0) tone(BUZZER_PIN, 1000, 200);
  } else {    // ALERTA
    setLED(true, false, false);
    if ((millis() / 1000) % 5 == 0) tone(BUZZER_PIN, 2000, 300);
  }
}
