#pragma once

#include <AsyncMqttClient.h>
#include <WiFi.h>

extern AsyncMqttClient mqttClient;

const IPAddress mqttBroker(MQTT_HOST);
const uint16_t mqttPort = MQTT_PORT;
const char* mqttUser = MQTT_USER;
const char* mqttPassword = MQTT_PASSWORD;
const char* mqttClientId = MQTT_CLIENT_ID;
const char* mqttTopic = MQTT_TOPIC;
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
    uint16_t packetIdSub = mqttClient.subscribe(mqttTopic, mqttQos);
    Serial.print("Subscribing at QoS ");
    Serial.print(mqttQos);
    Serial.print(", packetId: ");
    Serial.println(packetIdSub);
}

void PublishMqtt(const String& payload, bool retain = true)
{
    if(!mqttClient.connected())
    {
        return;
    }

    mqttClient.publish(mqttTopic, mqttQos, retain, payload.c_str());
}
