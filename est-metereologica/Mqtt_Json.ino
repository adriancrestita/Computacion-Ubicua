#include <WiFi.h>
#include <ArduinoJson.h>

#include "config.h"  // Sustituir con datos de vuestra red
#include "ESP32_Utils.hpp"
#include "ESP32_Utils_MQTT_Async.hpp"
#include "MQTT.hpp"

void setup(void)
{
	Serial.begin(115200);

	delay(500);

        InitMqtt();
        mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
                Serial.print("Mensaje recibido en [");
                Serial.print(topic);
                Serial.print("]: ");
                for(unsigned int i = 0; i < length; ++i)
                {
                        Serial.print(static_cast<char>(payload[i]));
                }
                Serial.println();
        });

        ConnectWiFi_STA();
        EnsureMqttConnection();
}

void loop()
{
        HandleMqttLoop();

        StaticJsonDocument<256> jsonDoc;
        jsonDoc["mensaje"] = "Hola desde ESP32";
        jsonDoc["timestamp"] = millis();

        String payload;
        serializeJson(jsonDoc, payload);

        PublishDefaultTopic(payload);

        delay(10000);
}