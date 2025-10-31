/* ======================================================
 *  Proyecto: Estación Meteorológica Local con WiFi (ESP32)
 *  Descripción: Mide temperatura, humedad, presión, luz,
 *  velocidad del viento y calidad del aire.
 *  Muestra los datos en OLED, consola serial y los envía
 *  por WiFi (HTTP o MQTT/ThingSpeak).
 *  Autor: [Tu Nombre]
 *  Fecha: [Actualizar]
 *  ======================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>

// === Configuración WiFi ===
const char* ssid = "TuSSID";
const char* password = "TuPASSWORD";

// URL del servidor o endpoint ThingSpeak (opcional)
String serverName = "http://api.thingspeak.com/update";
String apiKey = "TU_API_KEY";  // si usas ThingSpeak

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
const int RecordTime = 3;                // Tiempo de muestreo (s)
volatile int InterruptCounter = 0;       // Contador de interrupciones
volatile unsigned long lastPulse = 0;    // Tiempo del último pulso (µs)
float WindSpeed = 0.0;                   // Velocidad del viento en km/h
const float ANEMO_FACTOR = 2.4;          // Factor de conversión a km/h

// === Sensores ===
Adafruit_BMP085 bmp;
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

// === Pantalla OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === MQ2 ===
const int MQ2_umbral_base = 1400;
const int MQ2_umbral_malo = 1700;
const int MQ2_umbral_horrible = 1800;

// === Prototipos ===
void renderDisplay(void (*pageFunc)());
void showResumen();
String getCalidadAire(int gasADC);
void measureWind();
void sendToServer(float temp, float hum, float pres, float alt, float lux, float wind, int gasA, String calidad);
void connectWiFi();
void IRAM_ATTR countup();

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
  Serial.println("🌦 Iniciando Estación Meteorológica Local con WiFi...");

  Wire.begin(21, 22);

  // --- Sensores ---
  dht.begin();
  lightMeter.begin();

  if (!bmp.begin()) {
    Serial.println("❌ No se detecta BMP180/BMP085. Revisa conexiones.");
    while (true);
  }

  // --- OLED ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ No se pudo inicializar OLED.");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.println("Estacion con WiFi OK!");
  display.display();

  // --- Pines ---
  pinMode(BUTTON_BACK, INPUT_PULLDOWN);
  pinMode(BUTTON_NEXT, INPUT_PULLDOWN);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(ANEMO_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // --- WiFi ---
  connectWiFi();

  Serial.println("✅ Sistema iniciado correctamente.");
}

// =============================================================
// === LOOP PRINCIPAL ===
// =============================================================
void loop() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  float pres = bmp.readPressure() / 100.0;
  float altitud = bmp.readAltitude();
  float lux = lightMeter.readLightLevel();
  int gasA = analogRead(MQ2_AO);

  measureWind();
  String calidad = getCalidadAire(gasA);

  // --- Mostrar en consola ---
  Serial.println("===================================");
  Serial.printf("🌡 Temp: %.1f °C\n", temp);
  Serial.printf("💧 Humedad: %.1f %%\n", hum);
  Serial.printf("🧭 Presion: %.1f hPa\n", pres);
  Serial.printf("⛰ Altitud: %.1f m\n", altitud);
  Serial.printf("☀️  Luz: %.1f lx\n", lux);
  Serial.printf("🌬 Viento: %.1f km/h (%.2f m/s)\n", WindSpeed, WindSpeed / 3.6);
  Serial.printf("🧪 MQ2: %d (%s)\n", gasA, calidad.c_str());
  Serial.println("===================================");

  renderDisplay(showResumen);

  // --- Enviar por WiFi ---
  sendToServer(temp, hum, pres, altitud, lux, WindSpeed, gasA, calidad);

  delay(10000); // cada 10 segundos
}

// =============================================================
// === FUNCIONES DE VISUALIZACIÓN OLED ===
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
// === FUNCIÓN: CALIDAD DEL AIRE MQ2 ===
// =============================================================
String getCalidadAire(int gasADC) {
  if (gasADC > MQ2_umbral_horrible) return "PELIGROSA";
  if (gasADC > MQ2_umbral_malo) return "MALA";
  if (gasADC > MQ2_umbral_base) return "NORMAL";
  return "BUENA";
}

// =============================================================
// === FUNCIÓN: CONEXIÓN WiFi ===
// =============================================================
void connectWiFi() {
  Serial.printf("🔌 Conectando a WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi conectado!");
    Serial.print("📶 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ No se pudo conectar a WiFi.");
  }
}

// =============================================================
// === FUNCIÓN: ENVÍO DE DATOS A SERVIDOR O THINGSPEAK ===
// =============================================================
void sendToServer(float temp, float hum, float pres, float alt, float lux, float wind, int gasA, String calidad) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = serverName + "?api_key=" + apiKey +
                 "&field1=" + String(temp, 1) +
                 "&field2=" + String(hum, 1) +
                 "&field3=" + String(pres, 1) +
                 "&field4=" + String(alt, 1) +
                 "&field5=" + String(lux, 1) +
                 "&field6=" + String(wind, 1) +
                 "&field7=" + String(gasA) +
                 "&field8=" + calidad;

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.printf("📡 Datos enviados correctamente (%d)\n", httpCode);
    } else {
      Serial.printf("⚠️ Error enviando datos: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("⚠️ WiFi no conectado, no se enviaron datos.");
  }
}