#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <math.h>

// ====== MQTT/WiFi (tus utilidades) ======
#include "Mqtt_Json/config.h"
#include "Mqtt_Json/ESP32_Utils.hpp"
#include "Mqtt_Json/ESP32_Utils_MQTT_Async.hpp"
#include "Mqtt_Json/MQTT.hpp"

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

// === INTERVALOS MQTT ===
const unsigned long INTERVAL_TEMP   = 60 * 1000;
const unsigned long INTERVAL_HUM    = 5 * 60 * 1000;
const unsigned long INTERVAL_PRES   = 60 * 60 * 1000;
const unsigned long INTERVAL_LUZ    = 30 * 1000;
const unsigned long INTERVAL_VIENTO = 30 * 1000;
const unsigned long INTERVAL_GAS    = 60 * 1000;

unsigned long lastTemp = 0, lastHum = 0, lastPres = 0, lastLuz = 0, lastViento = 0, lastGas = 0;

// === VARIABLES MQTT RECIBIDAS ===
float mqttTemp = NAN;
float mqttHum = NAN;
float mqttPres = NAN;
float mqttLuz = NAN;
float mqttWindSpeed = NAN;
float mqttGas = NAN;

// === PROTOTIPOS ===
void handleAlert(int estado);
void showDHT_aux_MQTT();
void showBMP_MQTT();
void showBH1750_MQTT();
void showAnemo_aux_MQTT();
void showMQ2_aux_MQTT();
void showResumen_MQTT();
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
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, msg)) return;

  String t = String(topic);

  if (t == "estacion/temperatura") mqttTemp = doc["temperature_celsius"];
  else if (t == "estacion/humedad") mqttHum = doc["humidity_percent"];
  else if (t == "estacion/presion") mqttPres = doc["atmospheric_pressure_hpa"];
  else if (t == "estacion/luz") mqttLuz = doc["lx"];
  else if (t == "estacion/viento") mqttWindSpeed = doc["wind_speed_kmh"];
  else if (t == "estacion/gas") mqttGas = doc["air_quality_index"];

  Serial.printf("📩 MQTT [%s]: %s\n", topic, msg.c_str());
}

// === PUBLICAR MQTT ===
void publicarSensor(const char* topic, const String& payload) {
  mqttClient.publish(topic, 0, true, payload.c_str());
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
  WiFi.onEvent(WiFiEvent);
  InitMqtt();
  mqttClient.onMessage(onMqttMessage);

  // Suscribirse a todos los topics
  mqttClient.subscribe("estacion/temperatura");
  mqttClient.subscribe("estacion/humedad");
  mqttClient.subscribe("estacion/presion");
  mqttClient.subscribe("estacion/luz");
  mqttClient.subscribe("estacion/viento");
  mqttClient.subscribe("estacion/gas");

  ConnectWiFi_STA(true);
  initTime();
}

// === LOOP ===
void loop() {
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

  if (now - lastTemp >= INTERVAL_TEMP) {
    lastTemp = now;
    publicarSensor("estacion/temperatura", buildSensorJsonFor("TEMP_001","temperature","ST_001",40.416775,-3.703790,altitud,"temperature_celsius",temp));
  }

  if (now - lastHum >= INTERVAL_HUM) {
    lastHum = now;
    publicarSensor("estacion/humedad", buildSensorJsonFor("HUM_001","humidity","ST_001",40.416775,-3.703790,altitud,"humidity_percent",hum));
  }

  if (now - lastPres >= INTERVAL_PRES) {
    lastPres = now;
    publicarSensor("estacion/presion", buildSensorJsonFor("PRES_001","pressure","ST_001",40.416775,-3.703790,altitud,"atmospheric_pressure_hpa",pres));
  }

  if (now - lastLuz >= INTERVAL_LUZ) {
    lastLuz = now;
    publicarSensor("estacion/luz", buildSensorJsonFor("LIGHT_001","light","ST_001",40.416775,-3.703790,altitud,"lx",lux));
  }

  if (now - lastViento >= INTERVAL_VIENTO) {
    lastViento = now;
    measureWind();
    publicarSensor("estacion/viento", buildSensorJsonFor("WIND_001","wind","ST_001",40.416775,-3.703790,altitud,"wind_speed_kmh",WindSpeed));
  }

  if (now - lastGas >= INTERVAL_GAS) {
    lastGas = now;
    publicarSensor("estacion/gas", buildSensorJsonFor("GAS_001","air_quality","ST_001",40.416775,-3.703790,altitud,"air_quality_index",gasA));
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
  else display.printf("Pres: %.1f hPa\n", mqttPres);
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
  if (isnan(mqttGas)) display.println("Sin datos MQTT");
  else {
    display.printf("MQTT: %.0f\n", mqttGas);
    display.printf("Calidad: %s", getCalidadAire((int)mqttGas).c_str());
  }
}

void showResumen_MQTT() {
  display.setTextSize(1);
  display.println("RESUMEN MQTT");
  display.println("--------------------");
  display.printf("Temp: %.1f C\n", mqttTemp);
  display.printf("Hum : %.1f %%\n", mqttHum);
  display.printf("Pres: %.1f hPa\n", mqttPres);
  display.printf("Luz : %.1f lx\n", mqttLuz);
  display.printf("Viento: %.1f km/h\n", mqttWindSpeed);
  display.printf("Gas: %.0f (%s)\n", mqttGas, getCalidadAire((int)mqttGas).c_str());
  display.println("--------------------");
}