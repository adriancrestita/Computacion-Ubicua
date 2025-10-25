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

<<<<<<< HEAD
unsigned long lastTemp = 0, lastHum = 0, lastPres = 0, lastLuz = 0, lastViento = 0, lastGas = 0;
=======
// Umbrales para MQ2 
const int MQ2_umbral_base = 1400; // Valor de referencia en aire limpio
const int MQ2_umbral_normal = 1500; 
const int MQ2_umbral_malo = 1700;   
const int MQ2_umbral_horrible = 1800; // Peligroso (Rojo)
>>>>>>> a0262596b5b4abc415692f03594c24e817d528ac

// === VARIABLES MQTT RECIBIDAS ===
float mqttTemp = NAN;
float mqttHum = NAN;
float mqttPres = NAN;
float mqttLuz = NAN;
float mqttWindSpeed = NAN;
float mqttGas = NAN;

<<<<<<< HEAD
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
=======
// Umbrales de Presión Atmosférica (en hPa)
// REMOVIDO: const float P0 = 1013.25;
const float PRESION_ALTA_UMBRAL = 1025.0; 
const float PRESION_BAJA_UMBRAL = 1000.0; 
>>>>>>> a0262596b5b4abc415692f03594c24e817d528ac

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

<<<<<<< HEAD
  String t = String(topic);
=======
float calcularAltitud(float P_medida);  // Calcula la altitud a partir de la presión
String getGasDetectado(int gasADC); // Comprueba a ver si hay algún gas inflamable en el ambiente
String getCalidadAire(int gasADC); // Obtiene la calidad del aire segun un umbral de valores
>>>>>>> a0262596b5b4abc415692f03594c24e817d528ac

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
<<<<<<< HEAD
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
=======
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL

  // Inicializar sensores
  dht.begin();
  lightMeter.begin();
  if (!bmp.begin()) {
    Serial.println("Error al iniciar BMP180");
    while (1);
  }

  // Inicializar pantalla
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Error al iniciar pantalla OLED");
    while (1);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Configurar pines
  pinMode(MQ2_DO, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  Serial.println("Sistema de sensores iniciado correctamente.");
  delay(2000); 

  // Inicialización WIFI + MQTT
  Serial.println("Conectando WiFi y Broker MQTT...");
  Wifi.onEvent(WiFiEvent); 
  InitMqtt(); // Define los callbacks y timers MQTT
  ConnectWiFi_STA(true); // Conecta con IP estática definida en config.h
>>>>>>> a0262596b5b4abc415692f03594c24e817d528ac
}

// === LOOP ===
void loop() {
<<<<<<< HEAD
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
=======
  // === Botón con antirrebote ===
  bool buttonState = digitalRead(BUTTON_PIN);
  if (buttonState != lastButtonState) {
    if (buttonState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
      menuIndex++;
      // Mantenemos el límite de 5, si es 6 (5+1), vuelve a 0 (DHT11)
      if (menuIndex > 5) menuIndex = 0; 
      lastDebounceTime = millis();
    }
  }
  lastButtonState = buttonState;

  // === Lectura de sensores ===
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  float pres = bmp.readPressure() / 100.0; // Presión en hPa
  float lux = lightMeter.readLightLevel();
  int viento = analogRead(ANEMO_AO);
  int gasA = analogRead(MQ2_AO); // Lectura analógica del MQ2 (valores incrementales segun calidad de gas)
  int gasD = digitalRead(MQ2_DO); // Lectura digital (salida binaria si supera umbral puesto por potenciometro)

  // === Determinar estado ===
  // estado: 0=Normal/Verde, 1=Malo/Blanco, 2=Horrible/Buzzer
  int estado = 0;   

  // El estado de alerta debe calcularse solo para la vista actual (menuIndex)
  switch (menuIndex) {
    case 0:   // DHT11
      if (isnan(temp) || isnan(hum)) {
        estado = 2;
      } else {
        if (hum > 80 || temp > 40) estado = 2;
        else if (hum > 60 || temp > 30) estado = 1;
      }
      break;
      
    case 1:   // BMP180
      if (pres > PRESION_ALTA_UMBRAL) estado = 2; 
      else if (pres < PRESION_BAJA_UMBRAL) estado = 1; 
      else estado = 0; 
      break;
      
    case 2:   // BH1750 - LÓGICA DE 3 ESTADOS
      if (lux >= LUX_ALTO_UMBRAL) {
          estado = 1; // Mucha luz -> Estado 1 (LED Blanco)
      } else if (lux >= LUX_NORMAL_UMBRAL) {
          estado = 0; // Luz normal -> Estado 0 (LED Verde)
      } else { 
          estado = 2; // Poca luz / Oscuridad -> Estado 2 (Solo Buzzer)
      }
      break;
      
    case 3:   // Anemómetro
      if (viento > anemo_umbral_malo) estado = 2;
      else if (viento > anemo_umbral_normal) estado = 1;
      break;
      
    case 4:   // MQ2 
      if (gasA > MQ2_umbral_horrible) estado = 2; // Peligro extremo (Rojo)
      else if (gasA > MQ2_umbral_malo) estado = 1; // Alerta de gas/humo (Amarillo)
      else estado = 0; // Calidad Buena/Base (Verde)
      break;

    case 5: // Resumen (No tiene lógica de alerta directa)
      estado = 0; // Por defecto: Verde (normal) para la vista general.
      break;
      
  }

  // === Control de LED y buzzer ===
  handleAlert(estado); 

  // === Mostrar pantalla ===
  display.clearDisplay();
  display.setCursor(0, 0);
  switch (menuIndex) {
    case 0: showDHT_aux(temp, hum); break;
    case 1: showBMP(); break;
    case 2: showBH1750(); break; 
    case 3: showAnemo_aux(viento); break;
    case 4: showMQ2_aux(gasA, gasD); break;
    case 5: showResumen(); break; 
  }
  display.display();


  // Envio MQTT cada 5 segundos
  if (millis() - lastPublish >= intervaloEnvio) {
    lastPublish = millis();

    float altitud = calcularAltitud(pres);

    StaticJsonDocument<512> json;

    // Estructura con valor + unidad
    JsonObject temperatura = json.createNestedObject("temperatura");
    temperatura["valor"] = temp;
    temperatura["unidad"] = "°C";

    JsonObject humedad = json.createNestedObject("humedad");
    humedad["valor"] = hum;
    humedad["unidad"] = "%";

    JsonObject presion = json.createNestedObject("presion");
    presion["valor"] = pres;
    presion["unidad"] = "hPa";

    JsonObject luz = json.createNestedObject("luz");
    luz["valor"] = lux;
    luz["unidad"] = "lx";

    JsonObject vientoObj = json.createNestedObject("viento");
    vientoObj["valor"] = viento;
    vientoObj["unidad"] = "ADC";

    JsonObject gasObj = json.createNestedObject("gas");
    gasObj["valor"] = gasA;

    JsonObject altitudObj = json.createNestedObject("altitud");
    altitudObj["valor"] = altitud;
    altitudObj["unidad"] = "m";

    String payload;
    serializeJsonPretty(json, payload);

    mqttClient.publish("estacion/datos", 0, true, payload.c_str()); // (tópido donde publicar, QoS, indica si el ultimo valor añadido debe guardarse como el último añadido, mensaje a enviar)

    Serial.println("Publicado en MQTT:");
    Serial.println(payload);
  }
}

// === LED + buzzer según estado ===
void handleAlert(int estado) {
  
  // LÓGICA ESPECIAL PARA EL BH1750 (menuIndex == 2)
  if (menuIndex == 2) {
      if (estado == 1) { 
          // Mucha Luz (Blanco)
          digitalWrite(LED_R, HIGH);
          digitalWrite(LED_G, HIGH);
          digitalWrite(LED_B, HIGH);
          digitalWrite(BUZZER_PIN, LOW);
          return;
      } else if (estado == 2) {
          // Poca Luz/Oscuridad (Solo Buzzer - NUNCA LED)
          digitalWrite(LED_R, LOW);
          digitalWrite(LED_G, LOW);
          digitalWrite(LED_B, LOW);
          tone(BUZZER_PIN, 1500, 100); // Pitar
          return;
      }
      // El estado 0 se maneja abajo para el LED Verde
  }
  
  // Lógica de alerta estándar (incluye el estado 0 del BH1750 y todos los estados de otros sensores):
  if (estado == 0) {    
    // Normal (o Luz Normal si menuIndex == 2) -> LED Verde
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_B, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  } 
  else if (estado == 1) {   
    // Malo (Amarillo/Naranja) - Para otros sensores
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_B, LOW);
    if ((millis() / 1000) % 20 == 0) tone(BUZZER_PIN, 1000, 200);
  } 
  else {    
    // Horrible (Rojo) - Para otros sensores
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_B, LOW);
    if ((millis() / 1000) % 5 == 0) tone(BUZZER_PIN, 2000, 300);
  }
}

// === Funciones auxiliares ===
/**
 * @brief Calcula la altitud aproximada (en metros)
 *        usando la ecuación barométrica estándar (ISA).
 * @param P_medida    Presión actual medida (hPa)
 * @param P_referencia Presión de referencia a nivel del mar (hPa)
 * @return Altitud estimada (m)
 */
float calcularAltitud(float P_medida) {
    const float P_ESTANDAR_REFERENCIA = 1013.25;
    return (P_ESTANDAR_REFERENCIA - P_medida) * 8.0;
}

/**
 * @brief Estima el tipo de gas predominante según la sensibilidad del MQ-2.
 *        Se basa en la respuesta típica del sensor frente a GLP, humo y CO/metano.
 * @param gasADC Valor analógico leído (0-4095 en ESP32)
 * @return String con el nombre del gas más probable detectado.
 */
String getGasDetectado(int gasADC) {
    if (gasADC >= MQ2_umbral_horrible) {
        return "GLP/Propano"; 
    } 
    else if (gasADC >= MQ2_umbral_malo) {
        return "Humo";
    } 
    else if (gasADC >= MQ2_umbral_base) {
        return "CO/Metano";
    }
    return "Limpio";
}

/**
 * @brief Evalúa la calidad del aire basándose en el valor analógico del MQ-2.
 * @param gasADC Valor analógico del sensor MQ-2 (0-4095)
 * @return String con la clasificación de la calidad del aire.
 */
String getCalidadAire(int gasADC) {
    if (gasADC >= MQ2_umbral_horrible) {
        return "PELIGROSA";
    } else if (gasADC >= MQ2_umbral_malo) {
        return "MALA";
    } else if (gasADC >= MQ2_umbral_base) {
        return "NORMAL";
    } else {
        return "BUENA";
    }
}

/**
 * @brief Muestra temperatura y humedad del DHT11.
 */
void showDHT_aux(float temp, float hum) {
  display.setTextSize(2);
  display.println("DHT11");
  display.setTextSize(1);
  if (isnan(temp) || isnan(hum)) {
    display.println("Error lectura");
  } else {
    display.printf("Temp: %.1f C\n", temp);
    display.printf("Hum : %.1f %%", hum);
  }
}

/**
 * @brief Muestra presión atmosférica (hPa) y altitud (m)
 *        a partir del sensor BMP180.
 */
void showBMP() {
  float pres = bmp.readPressure() / 100.0; // Presión en hPa
  float altitud = calcularAltitud(pres); // Altitud en metros

  display.setTextSize(2);
  display.println("BMP180");
  display.setTextSize(1);
  
  display.printf("Pres: %.1f hPa\n", pres);
  
  if (abs(altitud) < 5) { 
      display.printf("Alt: ~0 m");
  } else {
      display.printf("Alt: %.0f m", altitud);
  }
}

/**
 * @brief Muestra el nivel de luz ambiental (lux)
 *        medido por el sensor BH1750.
 */
void showBH1750() {
  float lux = lightMeter.readLightLevel();
  display.setTextSize(2);
  display.println("BH1750");
  display.setTextSize(1);
  display.printf("Luz: %.1f lx", lux);
}

/**
 * @brief Muestra la lectura del anemómetro (valor ADC crudo).
 *        Podría calibrarse más adelante para mostrar velocidad del viento.
 */
void showAnemo_aux(int viento) {
  display.setTextSize(2);
  display.println("Viento");
  display.setTextSize(1);
  display.printf("ADC: %d", viento);
}

/**
 * @brief Muestra los valores del sensor MQ-2:
 *        valor ADC, gas detectado y calidad del aire.
 */
void showMQ2_aux(int gasA, int gasD) {
  display.setTextSize(2);
  display.println("MQ2");
  display.setTextSize(1);
  
  // Valor numérico
  display.printf("ADC: %d\n", gasA);
  
  // Gas detectado
  display.printf("Gas: %s\n", getGasDetectado(gasA).c_str());

  // Calidad del aire
  display.printf("Calidad: %s", getCalidadAire(gasA).c_str());
}

/**
 * @brief Muestra un resumen general con todos los sensores.
 *        Incluye temperatura, humedad, presión, luz, viento, gas y altitud.
 */
void showResumen() {
  // Limpiar pantalla antes de escribir
  display.clearDisplay();

  // Lecturas actualizadas
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  float pres = bmp.readPressure() / 100.0;
  float lux = lightMeter.readLightLevel();
  int viento = analogRead(ANEMO_AO);
  int gasA = analogRead(MQ2_AO); 
  
  display.setTextSize(1);
  display.println("ESTADO GENERAL (5/5)");
  display.println("--------------------");

  // Temperatura y humedad
  if (isnan(temp) || isnan(hum)) {
    display.println("T/H: ERROR");
  } else {
    display.printf("T: %.1f C | H: %.1f %%\n", temp, hum);
  }

  // Presión y luz
  display.printf("Pres: %.1f hPa\n", pres);
  display.printf("Luz: %.0f lx\n", lux);

  // Viento y gas
  display.printf("Viento: %d ADC\n", viento);
  display.printf("Gas: %d ADC (%s)\n", gasA, getCalidadAire(gasA).c_str());
  
  // Altitud
  float altitud = calcularAltitud(pres);
  if (abs(altitud) < 5) { 
      display.println("Alt: ~0 m");
  } else {
      display.printf("Alt: %.0f m\n", altitud);
  }
  
  display.println("--------------------");
>>>>>>> a0262596b5b4abc415692f03594c24e817d528ac
}