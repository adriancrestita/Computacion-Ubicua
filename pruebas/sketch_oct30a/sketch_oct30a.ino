/* ======================================================
 *  Proyecto: Estación Meteorológica Local con ESP32
 *  Descripción: Mide temperatura, humedad, presión, luz, 
 *  velocidad del viento y calidad del aire.
 *  Muestra los datos por pantalla OLED y consola serial.
 *  Autor: [Tu Nombre]
 *  Fecha: [Actualizar Fecha Actual]
 *  ======================================================
 */

#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>

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
const int RecordTime = 3;                // Tiempo de muestreo (s)
volatile int InterruptCounter = 0;       // Contador de interrupciones
volatile unsigned long lastPulse = 0;    // Tiempo del último pulso (µs)
float WindSpeed = 0.0;                   // Velocidad del viento en km/h
const float ANEMO_FACTOR = 2.4;          // Factor de conversión a km/h

// === Sensores conectados ===
Adafruit_BMP085 bmp;                     // Presión y altitud
BH1750 lightMeter;                       // Luz
DHT dht(DHTPIN, DHTTYPE);                // Temperatura y humedad

// === Configuración de pantalla OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === Umbrales MQ2 ===
const int MQ2_umbral_base = 1400;
const int MQ2_umbral_malo = 1700;
const int MQ2_umbral_horrible = 1800;

// === Prototipos ===
void renderDisplay(void (*pageFunc)());
void showResumen();
String getCalidadAire(int gasADC);
void measureWind();
void IRAM_ATTR countup();

// =============================================================
// === INTERRUPCIÓN: contar pulsos de viento ===
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
void measureWind() {
  InterruptCounter = 0;
  attachInterrupt(digitalPinToInterrupt(ANEMO_PIN), countup, FALLING);
  delay(RecordTime * 1000);
  detachInterrupt(digitalPinToInterrupt(ANEMO_PIN));
  WindSpeed = (float)InterruptCounter / (float)RecordTime * ANEMO_FACTOR;
}

// =============================================================
// === SETUP ===
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("🌦 Iniciando Estación Meteorológica Local...");

  Wire.begin(21, 22);

  // --- Inicialización de sensores ---
  dht.begin();
  lightMeter.begin();

  if (!bmp.begin()) {
    Serial.println("❌ No se detecta BMP180/BMP085. Revisa conexiones.");
    while (true);
  }

  // --- Pantalla OLED ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ No se pudo inicializar OLED.");
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.println("Estacion OK!");
  display.display();

  // --- Pines ---
  pinMode(BUTTON_BACK, INPUT_PULLDOWN);
  pinMode(BUTTON_NEXT, INPUT_PULLDOWN);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(ANEMO_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("✅ Sistema iniciado correctamente.");
}

// =============================================================
// === LOOP PRINCIPAL ===
// =============================================================
void loop() {
  // --- Lecturas de sensores ---
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  bmp.readTemperature();  // dummy read
  bmp.readPressure();
  delay(100);

  float pres = bmp.readPressure() / 100.0;        // hPa
  float altitud = bmp.readAltitude();      // metros
  float lux = lightMeter.readLightLevel();        // lux
  int gasA = analogRead(MQ2_AO);                  // MQ2 analógico
  measureWind();                                  // velocidad viento

  // --- Mostrar por consola ---
  Serial.println("===================================");
  Serial.printf("🌡 Temp: %.1f °C\n", temp);
  Serial.printf("💧 Humedad: %.1f %%\n", hum);
  Serial.printf("🧭 Presion: %.1f hPa\n", pres);
  Serial.printf("⛰ Altitud: %.1f m\n", altitud);
  Serial.printf("☀️  Luz: %.1f lx\n", lux);
  Serial.printf("🌬 Viento: %.1f km/h (%.2f m/s)\n", WindSpeed, WindSpeed / 3.6);
  Serial.printf("🧪 MQ2: %d (%s)\n", gasA, getCalidadAire(gasA).c_str());
  Serial.println("===================================");

  // --- Mostrar en pantalla OLED ---
  renderDisplay(showResumen);

  delay(5000); // actualiza cada 5 segundos
}

// =============================================================
// === FUNCIÓN DE VISUALIZACIÓN OLED ===
// =============================================================
void showResumen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("== Estacion Local ==");

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  float pres = bmp.readPressure() / 100.0;
  float altitud = bmp.readAltitude();
  float lux = lightMeter.readLightLevel();
  int gasA = analogRead(MQ2_AO);
  String calidad = getCalidadAire(gasA);

  display.printf("T: %.1f C  H: %.1f%%\n", temp, hum);
  display.printf("Pres: %.1f hPa\n", pres);
  display.printf("Alt: %.1f m\n", altitud);
  display.printf("Luz: %.1f lx\n", lux);
  display.printf("Viento: %.1f km/h\n", WindSpeed);
  display.printf("Gas: %d (%s)\n", gasA, calidad.c_str());
}

void renderDisplay(void (*pageFunc)()) {
  display.clearDisplay();
  pageFunc();
  display.display();
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