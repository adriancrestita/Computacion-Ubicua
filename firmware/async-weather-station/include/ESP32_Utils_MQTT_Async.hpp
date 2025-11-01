#pragma once

#include <WiFi.h>
#include "ESP32_Utils.hpp"
#include "MQTT.hpp"

extern AsyncMqttClient mqttClient;
extern void OnMqttReceived(char* topic,
                           char* payload,
                           AsyncMqttClientMessageProperties properties,
                           size_t len,
                           size_t index,
                           size_t total);

unsigned long lastMqttRetry = 0;
bool needMqttReconnect = false;
bool wifiConnected = false;
bool mqttConnecting = false;

void DebugPrintNetwork() {
    Serial.println("=== WiFi DEBUG ===");
    Serial.printf("WiFi.status(): %d\n", WiFi.status());
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("Local IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
    Serial.println("==================");
}

// =====================
// === MQTT connect ====
// =====================
void ConnectToMqtt() {
    if (!WiFi.isConnected()) {
        Serial.println("⚠️ MQTT skipped: no WiFi");
        return;
    }

    Serial.printf("🔌 Resolviendo broker: %s\n", mqttBroker);
    IPAddress resolvedIP;
    if (resolvedIP.fromString(mqttBroker)) {
        Serial.printf("Broker es IP directa: %s\n", resolvedIP.toString().c_str());
    } else {
        if (WiFi.hostByName(mqttBroker, resolvedIP)) {
            Serial.printf("DNS ok: %s -> %s\n", mqttBroker, resolvedIP.toString().c_str());
        } else {
            Serial.printf("❌ Error resolviendo %s\n", mqttBroker);
            return;
        }
    }

    Serial.printf("🚀 Intentando conectar a MQTT %s:%u\n", resolvedIP.toString().c_str(), mqttPort);
    mqttConnecting = true;
    mqttClient.connect();
}

// =====================
// === WiFi events =====
// =====================
void WiFiEvent(WiFiEvent_t event) {
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.println("✅ WiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            wifiConnected = true;
            needMqttReconnect = true;  // programa conexión
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("⚠️ WiFi lost connection");
            wifiConnected = false;
            mqttClient.disconnect();
            break;

        default:
            break;
    }
}

// =====================
// === MQTT callbacks ===
// =====================
void OnMqttConnect(bool sessionPresent) {
    mqttConnecting = false;
    Serial.println("✅ Conectado al broker MQTT");
    Serial.print("Session present: ");
    Serial.println(sessionPresent);

    SuscribeMqtt();

    const char* payload = "Estación MQTT conectada correctamente";
    bool success = mqttClient.publish(MQTT_TOPIC, 1, false, payload);
    if (success)
        Serial.println("📡 Payload publicado correctamente");
    else
        Serial.println("[WARN] No se pudo publicar el payload MQTT.");
}

void OnMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    mqttConnecting = false;
    Serial.printf("❌ Disconnected from MQTT. Reason: %d\n", (int)reason);

    switch (reason) {
        case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED: Serial.println("TCP_DISCONNECTED"); break;
        case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION: Serial.println("MQTT_UNACCEPTABLE_PROTOCOL_VERSION"); break;
        case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED: Serial.println("MQTT_IDENTIFIER_REJECTED"); break;
        case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE: Serial.println("MQTT_SERVER_UNAVAILABLE"); break;
        case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS: Serial.println("MQTT_MALFORMED_CREDENTIALS"); break;
        case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED: Serial.println("MQTT_NOT_AUTHORIZED"); break;
        default: break;
    }

    if (wifiConnected) {
        Serial.println("Reintentando conexión MQTT en 3 segundos...");
        needMqttReconnect = true;
        lastMqttRetry = millis();
    }
}

void OnMqttSubscribe(uint16_t packetId, uint8_t qos) {
    Serial.printf("Subscribe acknowledged. packetId: %d, qos: %d\n", packetId, qos);
}

void OnMqttUnsubscribe(uint16_t packetId) {
    Serial.printf("Unsubscribe acknowledged. packetId: %d\n", packetId);
}

void OnMqttPublish(uint16_t packetId) {
    Serial.printf("Publish acknowledged. packetId: %d\n", packetId);
}

// =====================
// === Inicialización ===
// =====================
void InitMqtt() {
    mqttClient.onConnect(OnMqttConnect);
    mqttClient.onDisconnect(OnMqttDisconnect);
    mqttClient.onSubscribe(OnMqttSubscribe);
    mqttClient.onUnsubscribe(OnMqttUnsubscribe);
    mqttClient.onMessage(OnMqttReceived);
    mqttClient.onPublish(OnMqttPublish);

    mqttClient.setServer(mqttBroker, mqttPort);
    mqttClient.setClientId(mqttClientId);
    mqttClient.setCredentials(mqttUser, mqttPassword);

    Serial.println("MQTT client initialized.");
}

// ============================
// === Handler desde loop() ===
// ============================
void HandleMqttTasks() {
    if (!wifiConnected) return;

    // cada 5 segundos, intenta conectar si hace falta
    if (needMqttReconnect && !mqttClient.connected() && !mqttConnecting) {
        if (millis() - lastMqttRetry > 5000) {
            lastMqttRetry = millis();
            Serial.println("🔁 Reintentando conexión MQTT...");
            DebugPrintNetwork();
            ConnectToMqtt();
        }
    }
}