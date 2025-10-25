#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <math.h> 

// === LIBRERÍAS DE COMUNICACIÓN ===
#include <WiFi.h> 
#include <ArduinoJson.h> 
#include <AsyncTCP.h> 
#include <AsyncMqttClient.h> 

// === ARCHIVOS AUXILIARES (DEBEN ESTAR EN LA MISMA CARPETA) ===
#include "config.h" 
#include "ESP32_Utils.hpp"
#include "ESP32_Utils_MQTT_Async.hpp"
#include "MQTT.hpp"
#include "JsonBuilder.hpp" 
// =================================================================

// === DEFINICIÓN DEL CLIENTE MQTT ===
AsyncMqttClient mqttClient; 

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
#define ANEMO_PIN 17 

// === ANEMÓMETRO (interrupción) ===
const int SensorPin = ANEMO_PIN;
const int RecordTime = 3;       
volatile int InterruptCounter = 0;
float WindSpeed = 0.0; // Velocidad del viento en km/h
const float ANEMO_FACTOR = 2.4; // Factor para calcular km/h

// === OBJETOS DE SENSORES ===
Adafruit_BMP085 bmp;
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

// === PANTALLA OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === VARIABLES INTERFAZ ===
int menuIndex = 5; // INICIA EN RESUMEN (índice 5)
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 300;
bool lastButtonBack = LOW;
bool lastButtonNext = LOW;

// === INTERVALOS Y VARIABLES DE ESTADO MQTT ===
const unsigned long INTERVAL_TEMP   = 60 * 1000;
const unsigned long INTERVAL_HUM    = 5 * 60 * 1000;
const unsigned long INTERVAL_PRES   = 60 * 60 * 1000;
const unsigned long INTERVAL_LUZ    = 30 * 1000;
const unsigned long INTERVAL_VIENTO = 30 * 1000;
const unsigned long INTERVAL_GAS    = 60 * 1000;
unsigned long lastTemp = 0, lastHum = 0, lastPres = 0, lastLuz = 0, lastViento = 0, lastGas = 0;

// VARIABLES MQTT RECIBIDAS (Declaradas aquí para el archivo .ino)
float mqttTemp = NAN;
float mqttHum = NAN;
float mqttPres = NAN;
float mqttLuz = NAN;
float mqttWindSpeed = NAN;
float mqttGas = NAN;


// === UMBRALES Y CONSTANTES ===
const int MQ2_umbral_base = 1400; 
const int MQ2_umbral_normal = 1500;
const int MQ2_umbral_malo = 1700;
const int MQ2_umbral_horrible = 1800; 

const float VIENTO_UMBRAL_MALO = 5.0; 
const float VIENTO_UMBRAL_HORRIBLE = 10.0;

const float PRESION_ALTA_UMBRAL = 1025.0; 
const float PRESION_BAJA_UMBRAL = 1000.0; 

const float LUX_NORMAL_UMBRAL = 50.0;
const float LUX_ALTO_UMBRAL = 300.0;
const float LUX_OSCURIDAD_UMBRAL = 10.0;

// === PROTOTIPOS ===
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

// === FUNCIÓN DE INTERRUPCIÓN ANEMÓMETRO ===
void IRAM_ATTR countup() { InterruptCounter++; }

void measureWind() {
  InterruptCounter = 0;
  attachInterrupt(digitalPinToInterrupt(SensorPin), countup, RISING);
  delay(RecordTime * 1000);
  detachInterrupt(SensorPin);
  WindSpeed = (float)InterruptCounter / (float)RecordTime * ANEMO_FACTOR;
}


// === CÁLCULO DE ALTITUD (Fórmula lineal simplificada) ===
float calcularAltitud(float P_medida, float P_referencia);
// =========================================================

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
  
  // === WIFI + MQTT ===
  WiFi.onEvent(WiFiEvent); 
  InitMqtt();             
  mqttClient.onMessage(onMqttMessage); 

  // Suscribirse a todos los topics 
  mqttClient.subscribe(MQTT_BASE_TOPIC "/temperatura", 0);
  mqttClient.subscribe(MQTT_BASE_TOPIC "/humedad", 0);
  mqttClient.subscribe(MQTT_BASE_TOPIC "/presion", 0);
  mqttClient.subscribe(MQTT_BASE_TOPIC "/luz", 0);
  mqttClient.subscribe(MQTT_BASE_TOPIC "/viento", 0);
  mqttClient.subscribe(MQTT_BASE_TOPIC "/gas", 0);

  ConnectWiFi_STA(true); 
  initTime(); 

  // Corrección para que el resumen sea la primera pantalla
  display.clearDisplay();
  showResumen_MQTT(); 
  display.display();
  lastButtonBack = digitalRead(BUTTON_BACK);
  lastButtonNext = digitalRead(BUTTON_NEXT);
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
  
  if (now - lastViento >= INTERVAL_VIENTO) {
      measureWind();
  }
  
  float altitud = calcularAltitud(pres, 1013.25); 

  // Publicación de Temperatura
  if (now - lastTemp >= INTERVAL_TEMP) {
    lastTemp = now;
    publicarSensor(MQTT_BASE_TOPIC "/temperatura", buildSensorJsonFor("TEMP_001","temperature","ST_001",40.416775,-3.703790,altitud,"temperature_celsius",temp));
  }

  // Publicación de Humedad
  if (now - lastHum >= INTERVAL_HUM) {
    lastHum = now;
    publicarSensor(MQTT_BASE_TOPIC "/humedad", buildSensorJsonFor("HUM_001","humidity","ST_001",40.416775,-3.703790,altitud,"humidity_percent",hum));
  }

  // Publicación de Presión
  if (now - lastPres >= INTERVAL_PRES) {
    lastPres = now;
    publicarSensor(MQTT_BASE_TOPIC "/presion", buildSensorJsonFor("PRES_001","pressure","ST_001",40.416775,-3.703790,altitud,"atmospheric_pressure_hpa",pres));
  }

  // Publicación de Luz
  if (now - lastLuz >= INTERVAL_LUZ) {
    lastLuz = now;
    publicarSensor(MQTT_BASE_TOPIC "/luz", buildSensorJsonFor("LIGHT_001","light","ST_001",40.416775,-3.703790,altitud,"lx",lux));
  }

  // Publicación de Viento
  if (now - lastViento >= INTERVAL_VIENTO) {
    publicarSensor(MQTT_BASE_TOPIC "/viento", buildSensorJsonFor("WIND_001","wind","ST_001",40.416775,-3.703790,altitud,"wind_speed_kmh",WindSpeed));
  }

  // Publicación de Gas
  if (now - lastGas >= INTERVAL_GAS) {
    lastGas = now;
    publicarSensor(MQTT_BASE_TOPIC "/gas", buildSensorJsonFor("GAS_001","air_quality","ST_001",40.416775,-3.703790,altitud,"air_quality_index",gasA));
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

// === MQTT CALLBACK ===
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, msg)) return;
  String t = String(topic);

  if (t == MQTT_BASE_TOPIC "/temperatura") mqttTemp = doc["data"]["temperature_celsius"];
  else if (t == MQTT_BASE_TOPIC "/humedad") mqttHum = doc["data"]["humidity_percent"];
  else if (t == MQTT_BASE_TOPIC "/presion") mqttPres = doc["data"]["atmospheric_pressure_hpa"];
  else if (t == MQTT_BASE_TOPIC "/luz") mqttLuz = doc["data"]["lx"];
  else if (t == MQTT_BASE_TOPIC "/viento") mqttWindSpeed = doc["data"]["wind_speed_kmh"];
  else if (t == MQTT_BASE_TOPIC "/gas") mqttGas = doc["data"]["air_quality_index"];

  Serial.printf("📩 MQTT [%s]: %s\n", topic, msg.c_str());
}

// === PUBLICAR MQTT ===
void publicarSensor(const char* topic, const String& payload) {
  mqttClient.publish(topic, 0, true, payload.c_str());
  Serial.printf("\n📡 Publicado en [%s]: %s\n", topic, payload.c_str());
}

float calcularAltitud(float P_medida, float P_referencia) {
    const float P_ESTANDAR_REFERENCIA = 1013.25;
    return (P_ESTANDAR_REFERENCIA - P_medida) * 8.0;
}

String getCalidadAire(int gasADC) {
  if (gasADC > 1800) return "PELIGROSA";
  else if (gasADC > 1700) return "MALA";
  else if (gasADC > 1400) return "NORMAL";
  return "BUENA";
}

void showDHT_aux_MQTT() {
  display.setTextSize(2); display.println("DHT11 (MQTT)");
  display.setTextSize(1);
  if (isnan(mqttTemp) || isnan(mqttHum)) display.println("Sin datos MQTT");
  else {
    display.printf("Temp: %.1f C\n", mqttTemp);
    display.printf("Hum : %.1f %%", mqttHum);
  }
}

void showBMP_MQTT() {
  float altitud = calcularAltitud(mqttPres, 1013.25);
  display.setTextSize(2); display.println("BMP180 (MQTT)");
  display.setTextSize(1);
  if (isnan(mqttPres)) display.println("Sin datos MQTT");
  else {
    display.printf("Pres: %.1f hPa\n", mqttPres);
    display.printf("Alt: %.0f m", altitud);
  }
}

void showBH1750_MQTT() {
  display.setTextSize(2); display.println("BH1750 (MQTT)");
  display.setTextSize(1);
  if (isnan(mqttLuz)) display.println("Sin datos MQTT");
  else display.printf("Luz: %.1f lx", mqttLuz);
}

void showAnemo_aux_MQTT() {
  display.setTextSize(2); display.println("Viento (MQTT)");
  display.setTextSize(1);
  if (isnan(mqttWindSpeed)) display.println("Sin datos MQTT");
  else {
    display.printf("km/h: %.1f\n", mqttWindSpeed);
    display.printf("m/s: %.2f", mqttWindSpeed / 3.6);
  }
}

void showMQ2_aux_MQTT() {
  display.setTextSize(2); display.println("MQ2 (MQTT)");
  display.setTextSize(1);
  if (isnan(mqttGas)) display.println("Sin datos MQTT");
  else {
    display.printf("ADC: %.0f\n", mqttGas);
    display.printf("Calidad: %s", getCalidadAire((int)mqttGas).c_str());
  }
}

void showResumen_MQTT() {
  float altitud = calcularAltitud(mqttPres, 1013.25);
  display.setTextSize(1);
  display.println("RESUMEN MQTT (5/5)");
  display.println("--------------------");
  display.printf("T: %.1f C | H: %.1f %%\n", mqttTemp, mqttHum);
  display.printf("Pres: %.1f hPa\n", mqttPres);
  display.printf("Luz : %.1f lx\n", mqttLuz);
  display.printf("Viento: %.1f km/h\n", mqttWindSpeed);
  display.printf("Gas: %.0f (%s)\n", mqttGas, getCalidadAire((int)mqttGas).c_str());
  display.printf("Alt: %.0f m\n", altitud);
  display.println("--------------------");
}

void handleAlert(int estado) {
  if (menuIndex == 2) {
      if (estado == 1) { 
          digitalWrite(LED_R, HIGH);
          digitalWrite(LED_G, HIGH);
          digitalWrite(LED_B, HIGH);
          digitalWrite(BUZZER_PIN, LOW);
          return;
      } else if (estado == 2) {
          digitalWrite(LED_R, LOW);
          digitalWrite(LED_G, LOW);
          digitalWrite(LED_B, LOW);
          tone(BUZZER_PIN, 1500, 100); 
          return;
      }
  }
  
  if (estado == 0) {    
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_B, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  } 
  else if (estado == 1) {   
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_B, LOW);
    if ((millis() / 1000) % 20 == 0) tone(BUZZER_PIN, 1000, 200);
  } 
  else {    
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_B, LOW);
    if ((millis() / 1000) % 5 == 0) tone(BUZZER_PIN, 2000, 300);
  }
}