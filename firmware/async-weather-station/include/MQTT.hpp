#pragma once

#include <AsyncMqttClient.h>
#include <WiFi.h>

extern AsyncMqttClient mqttClient;

// ✅ usar const char* para hostname/IP textual
const char*   mqttBroker    = MQTT_HOST;
const uint16_t mqttPort     = MQTT_PORT;
const char*   mqttUser      = MQTT_USER;
const char*   mqttPassword  = MQTT_PASSWORD;
const char*   mqttClientId  = MQTT_CLIENT_ID;

#ifndef MQTT_BASE_TOPIC
#define MQTT_BASE_TOPIC MQTT_TOPIC
#endif
const char*   mqttBaseTopic    = MQTT_BASE_TOPIC;
const char*   mqttPublishTopic = MQTT_TOPIC;
const uint8_t mqttQos          = MQTT_QOS;

String GetPayloadContent(char* data, size_t len)
{
    String content; content.reserve(len);
    for (size_t i = 0; i < len; i++) content.concat(data[i]);
    return content;
}

void SuscribeMqtt()
{
    String commandTopic = String(mqttBaseTopic) + "/comandos";
    uint16_t packetIdSub = mqttClient.subscribe(commandTopic.c_str(), mqttQos);
    Serial.printf("Subscribing at QoS %d, packetId: %d\n", mqttQos, packetIdSub);
}

bool PublishMqtt(const String& payload, bool retain = true)
{
    if (!mqttClient.connected()) return false;
    return mqttClient.publish(mqttPublishTopic, mqttQos, retain, payload.c_str());
}