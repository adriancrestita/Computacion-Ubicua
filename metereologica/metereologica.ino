#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>

// === Pines definidos según tu esquema ===
#define DHTPIN 14
#define DHTTYPE DHT11

#define HR31_DO 4
#define HR31_AO 36

#define ANEMO_AO 33

#define MQ2_DO 25
#define MQ2_AO 35

#define BUTTON_PIN 5

// === Objetos de sensores ===
Adafruit_BMP085 bmp;
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

// === Pantalla OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === Variables de menú ===
int menuIndex = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 300;
bool lastButtonState = LOW;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL

  // === Inicializar sensores ===
  dht.begin();
  lightMeter.begin();
  if (!bmp.begin()) {
    Serial.println("❌ Error al iniciar BMP180");
    while (1);
  }

  // === Inicializar pantalla ===
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ Error al iniciar pantalla OLED");
    while (1);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // === Pines digitales ===
  pinMode(HR31_DO, INPUT);
  pinMode(MQ2_DO, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);

  Serial.println("✅ Sistema de sensores iniciado correctamente.");
}

void loop() {
  // === Botón con antirrebote ===
  bool buttonState = digitalRead(BUTTON_PIN);
  if (buttonState != lastButtonState) {
    if (buttonState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
      menuIndex++;
      if (menuIndex > 5) menuIndex = 0;
      lastDebounceTime = millis();
    }
  }
  lastButtonState = buttonState;

  // === Mostrar según el menú ===
  display.clearDisplay();
  display.setCursor(0, 0);

  switch (menuIndex) {
    case 0: showMQ2(); break;
    case 1: showBMP(); break;
    case 2: showBH1750(); break;
    case 3: showHR31(); break;
    case 4: showAnemo(); break;
    case 5: showDHT(); break;
  }

  display.display();
  delay(300);
}

// === Funciones de cada sensor ===

void showDHT() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  display.setTextSize(2);
  display.println("DHT11");
  display.setTextSize(1);
  display.printf("Temp: %.1f C\n", temp);
  display.printf("Hum : %.1f %%", hum);
}

void showBMP() {
  float temp = bmp.readTemperature();
  float pres = bmp.readPressure() / 100.0;
  display.setTextSize(2);
  display.println("BMP180");
  display.setTextSize(1);
  display.printf("Temp: %.1f C\n", temp);
  display.printf("Pres: %.1f hPa", pres);
}

void showBH1750() {
  float lux = lightMeter.readLightLevel();
  display.setTextSize(2);
  display.println("BH1750");
  display.setTextSize(1);
  display.printf("Luz: %.1f lx", lux);
}

void showHR31() {
  int lluviaA = analogRead(HR31_AO);
  int lluviaD = digitalRead(HR31_DO);
  display.setTextSize(2);
  display.println("HR31");
  display.setTextSize(1);
  display.printf("Anal.: %d\n", lluviaA);
  display.printf("Digital: %s", lluviaD ? "Seco" : "Mojado");
}

void showAnemo() {
  int viento = analogRead(ANEMO_AO);
  display.setTextSize(2);
  display.println("Viento");
  display.setTextSize(1);
  display.printf("Valor ADC: %d", viento);
}

void showMQ2() {
  int gasA = analogRead(MQ2_AO);
  int gasD = digitalRead(MQ2_DO);
  display.setTextSize(2);
  display.println("MQ2");
  display.setTextSize(1);
  display.printf("Anal.: %d\n", gasA);
  display.printf("Digital: %s", gasD ? "Normal" : "Gas!");
}
