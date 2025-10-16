#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <math.h> 

// === Pines definidos ===
#define DHTPIN 14
#define DHTTYPE DHT11

#define ANEMO_AO 16
#define MQ2_DO 25
#define MQ2_AO 35 // Pin analógico para el MQ2
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
int menuIndex = 5; // MODIFICADO: INICIALIZADO A 5 para la vista de Resumen.
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 300;
bool lastButtonState = LOW;

// === Umbrales (ajustables) ===
// Umbrales para MQ2 - BASE DE ~1400 ADC
const int MQ2_umbral_base = 1400; // Valor de referencia en aire limpio
const int MQ2_umbral_normal = 1500; 
const int MQ2_umbral_malo = 1700;   
const int MQ2_umbral_horrible = 1800; // Peligroso (Rojo)

// Umbrales para Anemómetro
const int anemo_umbral_normal = 200;
const int anemo_umbral_malo = 400;

// Umbrales de Presión Atmosférica (en hPa)
const float P0 = 1013.25; 
const float PRESION_ALTA_UMBRAL = 1025.0; 
const float PRESION_BAJA_UMBRAL = 1000.0; 

// === Umbrales de LUZ (BH1750) ===
const float LUX_NORMAL_UMBRAL = 50.0;
const float LUX_ALTO_UMBRAL = 300.0;
const float LUX_OSCURIDAD_UMBRAL = 10.0;


// === Declaración de funciones (para mantener el orden) ===
void handleAlert(int estado);
void showDHT_aux(float temp, float hum);
void showBMP();
void showBH1750();
void showAnemo_aux(int viento);
void showMQ2_aux(int gasA, int gasD);
float calcularAltitud(float P_medida, float P_referencia); 
String getGasDetectado(int gasADC); 
String getCalidadAire(int gasADC); 

// === NUEVA FUNCIÓN AUXILIAR ===
void showResumen(); 

// ===========================================

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

  Serial.println("✅ Sistema de sensores iniciado correctamente. Calentando MQ-2...");
  delay(2000); 
}

void loop() {
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
  int gasA = analogRead(MQ2_AO); // Lectura analógica del MQ2
  int gasD = digitalRead(MQ2_DO);

  // === Determinar estado ===
  // estado: 0=Normal/Verde, 1=Malo/Blanco, 2=Horrible/Buzzer
  int estado = 0;   

  // El estado de alerta debe calcularse solo para la vista actual (menuIndex)
  switch (menuIndex) {
    case 0:   // DHT11
      if (isnan(temp) || isnan(hum)) {
        estado = 2;
      } else {
        if (hum > 80 || temp > 40) estado = 2;
        else if (hum > 60 || temp > 30) estado = 1;
      }
      break;
      
    case 1:   // BMP180
      if (pres > PRESION_ALTA_UMBRAL) estado = 2; 
      else if (pres < PRESION_BAJA_UMBRAL) estado = 1; 
      else estado = 0; 
      break;
      
    case 2:   // BH1750 - LÓGICA DE 3 ESTADOS
      if (lux >= LUX_ALTO_UMBRAL) {
          estado = 1; // Mucha luz -> Estado 1 (LED Blanco)
      } else if (lux >= LUX_NORMAL_UMBRAL) {
          estado = 0; // Luz normal -> Estado 0 (LED Verde)
      } else { 
          estado = 2; // Poca luz / Oscuridad -> Estado 2 (Solo Buzzer)
      }
      break;
      
    case 3:   // Anemómetro
      if (viento > anemo_umbral_malo) estado = 2;
      else if (viento > anemo_umbral_normal) estado = 1;
      break;
      
    case 4:   // MQ2 
      if (gasA > MQ2_umbral_horrible) estado = 2; // Peligro extremo (Rojo)
      else if (gasA > MQ2_umbral_malo) estado = 1; // Alerta de gas/humo (Amarillo)
      else if (gasA > MQ2_umbral_normal) estado = 0; // Calidad Normal (Verde)
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

  delay(300);
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
 * @brief Calcula la altitud aproximada (en metros).
 */
float calcularAltitud(float P_medida, float P_referencia) {
    return 44330.0 * (1.0 - pow((P_medida / P_referencia), 0.19029));
}

/**
 * @brief Estima el gas con mayor presencia basándose en la sensibilidad del MQ-2.
 */
String getGasDetectado(int gasADC) {
    if (gasADC >= MQ2_umbral_horrible) {
        return "GLP/Propano"; 
    } 
    else if (gasADC >= MQ2_umbral_malo) {
        return "Humo";
    } 
    else if (gasADC > MQ2_umbral_base) {
        return "CO/Metano";
    }
    return "Limpio";
}

/**
 * @brief Evalúa la calidad del aire basándose en la lectura analógica.
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
  float pres = bmp.readPressure() / 100.0; // Presión en hPa
  float altitud = calcularAltitud(pres, P0); // Altitud aproximada en metros

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
  
  // Imprimir valor numérico
  display.printf("ADC: %d\n", gasA);
  
  // Imprimir Gas Detectado
  display.printf("Gas: %s\n", getGasDetectado(gasA).c_str());

  // Imprimir Calidad Aproximada
  display.printf("Calidad: %s", getCalidadAire(gasA).c_str());
}

// === FUNCIÓN DE RESUMEN ===
void showResumen() {
  // Lecturas frescas para el resumen
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  float pres = bmp.readPressure() / 100.0;
  float lux = lightMeter.readLightLevel();
  int viento = analogRead(ANEMO_AO);
  int gasA = analogRead(MQ2_AO); 
  
  display.setTextSize(1);
  display.println("ESTADO GENERAL (5/5)");
  display.println("--------------------");

  // Línea 1: T/H
  if (isnan(temp) || isnan(hum)) {
    display.println("T/H: ERROR");
  } else {
    display.printf("T: %.1f C | H: %.1f %%\n", temp, hum);
  }

  // Línea 2: Presión/Luz
  display.printf("Pres: %.1f hPa\n", pres);
  display.printf("Luz: %.0f lx\n", lux);

  // Línea 3: Viento/Gas
  display.printf("Viento: %d ADC\n", viento);
  display.printf("Gas: %d ADC (%s)\n", gasA, getCalidadAire(gasA).c_str());
  
  // Línea 4: Espacio para Altitud o Mensaje
  float altitud = calcularAltitud(pres, P0);
  if (abs(altitud) < 5) { 
      display.println("Alt: ~0 m");
  } else {
      display.printf("Alt: %.0f m\n", altitud);
  }
  
  display.println("--------------------");
}