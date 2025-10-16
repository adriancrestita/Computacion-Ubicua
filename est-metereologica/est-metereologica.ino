#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <math.h> 



// === PINES DEFINIDOS ===
#define DHTPIN 14
#define DHTTYPE DHT11
#define ANEMO_AO 16
#define MQ2_DO 25
#define MQ2_AO 35 
#define BUTTON_PIN 5
#define BUZZER_PIN 15
#define LED_R 12
#define LED_G 13
#define LED_B 27

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
bool lastButtonState = LOW;

// === UMBRALES Y CONSTANTES ===

// Umbrales para MQ2 
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

// === Declaración de funciones ===
void handleAlert(int estado); // Gestiona las alertas a emitir en función del estado

void showResumen(); // Muestra la pantalla inicial de información global
void showDHT_aux(float temp, float hum); // muestra los valores del DHT11
void showBMP(); // Muestra los valores del BMP180
void showBH1750(); // Muestra los valores del BH1750
void showAnemo_aux(int viento); // Muestra los valores obtenidos por el anemometro
void showMQ2_aux(int gasA, int gasD); // Muestra los valores del MQ2

float calcularAltitud(float P_medida, float P_referencia);  // Calcula la altitud a partir de la presión
String getGasDetectado(int gasADC); // Comprueba a ver si hay algún gas inflamable en el ambiente
String getCalidadAire(int gasADC); // Obtiene la calidad del aire segun un umbral de valores

unsigned long lastPublish = 0; // Ultimo envio a MQTT
const unsigned int intervaloEnvio = 5 * 1000; // Envio cada 5 segundos

// === SET UP === 
void setup() {
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
}

// === LOOP PRINCIPAL ===
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
  int gasA = analogRead(MQ2_AO); // Lectura analógica del MQ2 (valores incrementales segun calidad de gas)
  int gasD = digitalRead(MQ2_DO); // Lectura digital (salida binaria si supera umbral puesto por potenciometro)

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

    float altitud = calcularAltitud(pres, P0);

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
 *        usando la ecuación barométrica estándar (ISA).
 * @param P_medida    Presión actual medida (hPa)
 * @param P_referencia Presión de referencia a nivel del mar (hPa)
 * @return Altitud estimada (m)
 */
float calcularAltitud(float P_medida, float P_referencia) {
    return 44330.0 * (1.0 - pow((P_medida / P_referencia), 0.19029));
}

/**
 * @brief Estima el tipo de gas predominante según la sensibilidad del MQ-2.
 *        Se basa en la respuesta típica del sensor frente a GLP, humo y CO/metano.
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
 *        a partir del sensor BMP180.
 */
void showBMP() {
  float pres = bmp.readPressure() / 100.0; // Presión en hPa
  float altitud = calcularAltitud(pres, P0); // Altitud en metros

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
 *        medido por el sensor BH1750.
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
 *        Podría calibrarse más adelante para mostrar velocidad del viento.
 */
void showAnemo_aux(int viento) {
  display.setTextSize(2);
  display.println("Viento");
  display.setTextSize(1);
  display.printf("ADC: %d", viento);
}

/**
 * @brief Muestra los valores del sensor MQ-2:
 *        valor ADC, gas detectado y calidad del aire.
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
 *        Incluye temperatura, humedad, presión, luz, viento, gas y altitud.
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
  float altitud = calcularAltitud(pres, P0);
  if (abs(altitud) < 5) { 
      display.println("Alt: ~0 m");
  } else {
      display.printf("Alt: %.0f m\n", altitud);
  }
  
  display.println("--------------------");
}