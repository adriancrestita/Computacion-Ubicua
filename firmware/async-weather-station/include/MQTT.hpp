#pragma once

#include <AsyncMqttClient.h>
#include <WiFi.h>

extern AsyncMqttClient mqttClient;

const IPAddress mqttBroker(MQTT_HOST);
const uint16_t mqttPort = MQTT_PORT;
const char* mqttUser = MQTT_USER;
const char* mqttPassword = MQTT_PASSWORD;
const char* mqttClientId = MQTT_CLIENT_ID;
#ifndef MQTT_BASE_TOPIC
#define MQTT_BASE_TOPIC MQTT_TOPIC
#endif
const char* mqttBaseTopic = MQTT_BASE_TOPIC;
const char* mqttPublishTopic = MQTT_TOPIC;
const uint8_t mqttQos = MQTT_QOS;

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
    String commandTopic = String(mqttBaseTopic) + "/comandos";
    // Reemplazado: suscripción directa a MQTT_TOPIC. Ahora se usa MQTT_BASE_TOPIC para derivar el canal de comandos.
    uint16_t packetIdSub = mqttClient.subscribe(commandTopic.c_str(), mqttQos);
    Serial.print("Subscribing at QoS ");
    Serial.print(mqttQos);
    Serial.print(", packetId: ");
    Serial.println(packetIdSub);
}

bool PublishMqtt(const String& payload, bool retain = true)
{
    if(!mqttClient.connected())
    {
        // Eliminado: publish sin comprobar estado. Ahora se devuelve false cuando no hay conexión MQTT.
        return false;
    }

    bool queued = mqttClient.publish(mqttPublishTopic, mqttQos, retain, payload.c_str());
    return queued;
}
