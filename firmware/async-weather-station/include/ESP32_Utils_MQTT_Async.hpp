#pragma once
#include <freertos/timers.h> 
#include <WiFi.h>
#include "MQTT.hpp" // Para obtener BROKER_IP, BROKER_PORT y mqttClient
#include "ESP32_Utils.hpp" // Para obtener ConnectWiFi_STA

// === DECLARACIONES GLOBALES ===
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// === PROTOTIPOS ===
void ConnectToMqtt();
// Prototipo de ConnectWiFi_STA es redundante aquí, pero lo mantenemos si es necesario por el profesor.
void SuscribeMqtt();
void OnMqttConnect(bool sessionPresent);
void OnMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void OnMqttSubscribe(uint16_t packetId, uint8_t qos);
void OnMqttUnsubscribe(uint16_t packetId);
void OnMqttPublish(uint16_t packetId);
void OnMqttReceived(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);


void ConnectToMqtt()
{
	Serial.println("Connecting to MQTT...");
	mqttClient.connect(); 
}

// MODIFICADO: Usamos los nombres de eventos correctos para la API moderna del ESP32
void WiFiEvent(arduino_event_id_t event) 
{
	Serial.printf("[WiFi-event] event: %d\n", event);
	switch(event)
	{
	case ARDUINO_EVENT_WIFI_STA_GOT_IP: 
		Serial.println("WiFi connected");
		Serial.println("IP address: ");
		Serial.println(WiFi.localIP());
		// Detiene el temporizador de reconexión WiFi (si estaba activo)
		xTimerStop(wifiReconnectTimer, 0); 
		ConnectToMqtt();
		break;
	case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: 
		Serial.println("WiFi lost connection");
		xTimerStop(mqttReconnectTimer, 0); 
		xTimerStart(wifiReconnectTimer, 0);
		break;
    case ARDUINO_EVENT_WIFI_AP_START:
    case ARDUINO_EVENT_WIFI_AP_STOP:
    case ARDUINO_EVENT_WIFI_STA_START:
        break;
	}
}

void OnMqttConnect(bool sessionPresent)
{
	Serial.println("Connected to MQTT.");
	Serial.print("Session present: ");
	Serial.println(sessionPresent);
	SuscribeMqtt(); 
}

void OnMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
	Serial.println("Disconnected from MQTT.");

	if(WiFi.isConnected())
	{
		xTimerStart(mqttReconnectTimer, 0);
	}
}

void OnMqttSubscribe(uint16_t packetId, uint8_t qos)
{
	Serial.println("Subscribe acknowledged.");
	Serial.print("  packetId: ");
	Serial.println(packetId);
	Serial.print("  qos: ");
	Serial.println(qos);
}

void OnMqttUnsubscribe(uint16_t packetId)
{
	Serial.println("Unsubscribe acknowledged.");
	Serial.print("  packetId: ");
	Serial.println(packetId);
}

void OnMqttPublish(uint16_t packetId)
{
	Serial.println("Publish acknowledged.");
	Serial.print("  packetId: ");
	Serial.println(packetId);
}

void InitMqtt()
{
	mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(ConnectToMqtt));
	wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(5000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(ConnectWiFi_STA));

    // Asignación de Callbacks
	mqttClient.onConnect(OnMqttConnect);
	mqttClient.onDisconnect(OnMqttDisconnect);

	mqttClient.onSubscribe(OnMqttSubscribe);
	mqttClient.onUnsubscribe(OnMqttUnsubscribe);

	mqttClient.onPublish(OnMqttPublish);
    
    // Configura el servidor
	mqttClient.setServer(BROKER_IP, BROKER_PORT); 
}