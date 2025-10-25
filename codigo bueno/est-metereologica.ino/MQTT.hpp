#pragma once
#include <AsyncMqttClient.h>
#include <WiFi.h> // Necesario para IPAddress
#include "config.h" // Necesario para las macros MQTT_HOST y MQTT_PORT

// === DECLARACIONES GLOBALES ===
// NOTA: mqttClient está definido en est-metereologica.ino
extern AsyncMqttClient mqttClient;

// === CONSTANTES DE CONEXIÓN ===
// Usamos las macros de config.h para definir las constantes de IP
const IPAddress BROKER_IP(MQTT_HOST);
const int BROKER_PORT = MQTT_PORT;


String GetPayloadContent(char* data, size_t len)
{
	String content = "";
	for(size_t i = 0; i < len; i++)
	{
		content.concat(data[i]);
	}
	return content;
}

void SuscribeMqtt()
{
	// Usamos MQTT_BASE_TOPIC, asumiendo que está definido en config.h
	uint16_t packetIdSub = mqttClient.subscribe(MQTT_BASE_TOPIC "/comandos", 0); 
	Serial.print("Subscribing at QoS 0, packetId: ");
	Serial.println(packetIdSub);
}

void PublishMqtt()
{
	String payload = "";

	StaticJsonDocument<300> jsonDoc;
	jsonDoc["data"] = millis();
	serializeJson(jsonDoc, payload);

	mqttClient.publish("hello/world", 0, true, (char*)payload.c_str());
}

// NOTA: El callback OnMqttReceived se declara y define en est-metereologica.ino