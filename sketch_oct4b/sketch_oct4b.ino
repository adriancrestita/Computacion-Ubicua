#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>

// === Pines definidos ===
#define DHTPIN 14
#define DHTTYPE DHT11

#define ANEMO_AO 33
#define MQ2_DO 25
#define MQ2_AO 35
#define BUTTON_PIN 5

// Pines para buzzer y LED RGB
#define BUZZER_PIN 15
#define LED_R 12
#define LED_G 13
#define LED_B 27

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

// === Umbrales (ajustables) ===
const int MQ2_umbral_normal = 200;
const int MQ2_umbral_malo = 400;

const int anemo_umbral_normal = 200;
const int anemo_umbral_malo = 400;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL

  // Inicializar sensores
  dht.begin();
  lightMeter.begin();
  if (!bmp.begin()) {
    Serial.println("❌ Error al iniciar BMP180");
    while (1);
  }

  // Inicializar pantalla
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ Error al iniciar pantalla OLED");
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

  Serial.println("✅ Sistema de sensores iniciado correctamente.");
}

void loop() {
  // === Botón con antirrebote ===
  bool buttonState = digitalRead(BUTTON_PIN);
  if (buttonState != lastButtonState) {
    if (buttonState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
      menuIndex++;
      if (menuIndex > 4) menuIndex = 0;
      lastDebounceTime = millis();
    }
  }
  lastButtonState = buttonState;

  // === Lectura de sensores ===
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  float bmpTemp = bmp.readTemperature();
  float pres = bmp.readPressure() / 100.0;
  float lux = lightMeter.readLightLevel();
  int viento = analogRead(ANEMO_AO);
  int gasA = analogRead(MQ2_AO);
  int gasD = digitalRead(MQ2_DO);

  // === Determinar estado ===
  int estado = 0;  // 0 = normal, 1 = malo, 2 = horrible

  switch (menuIndex) {
    case 0:  // MQ2
      if (gasA > MQ2_umbral_malo) estado = 2;
      else if (gasA > MQ2_umbral_normal) estado = 1;
      break;
    case 1:  // BMP
      if (pres < 950) estado = 2;
      else if (pres < 980) estado = 1;
      break;
    case 2:  // BH1750
      if (lux < 10) estado = 2;
      else if (lux < 50) estado = 1;
      break;
    case 3:  // Anemómetro
      if (viento > anemo_umbral_malo) estado = 2;
      else if (viento > anemo_umbral_normal) estado = 1;
      break;
    case 4:  // DHT11
      if (isnan(temp) || isnan(hum)) {
        estado = 2;
      } else {
        if (hum > 80 || temp > 40) estado = 2;
        else if (hum > 60 || temp > 30) estado = 1;
      }
      break;
  }

  // === Control de LED y buzzer ===
  handleAlert(estado);

  // === Mostrar pantalla ===
  display.clearDisplay();
  display.setCursor(0, 0);
  switch (menuIndex) {
    case 0: showMQ2_aux(gasA, gasD); break;
    case 1: showBMP(); break;
    case 2: showBH1750(); break;
    case 3: showAnemo_aux(viento); break;
    case 4: showDHT_aux(temp, hum); break;
  }
  display.display();

  delay(300);
}

// === LED + buzzer según estado ===
void handleAlert(int estado) {
  if (estado == 0) {  // normal
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_B, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  } 
  else if (estado == 1) {  // malo
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_B, LOW);
    if ((millis() / 1000) % 20 == 0) tone(BUZZER_PIN, 1000, 200);
  } 
  else {  // horrible
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_B, LOW);
    if ((millis() / 1000) % 5 == 0) tone(BUZZER_PIN, 2000, 300);
  }
}

// === Funciones auxiliares ===
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

void showAnemo_aux(int viento) {
  display.setTextSize(2);
  display.println("Viento");
  display.setTextSize(1);
  display.printf("ADC: %d", viento);
}

void showMQ2_aux(int gasA, int gasD) {
  display.setTextSize(2);
  display.println("MQ2");
  display.setTextSize(1);
  display.printf("Anal: %d\n", gasA);
  display.printf("Dig: %s", gasD ? "Normal" : "Gas!");
}
