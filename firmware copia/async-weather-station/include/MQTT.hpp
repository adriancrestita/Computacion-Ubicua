#pragma once

#include <Arduino.h>
#include <AsyncMqttClient.h>

constexpr char MQTT_TOPIC[] = "sensors/street_1253/WT_001";

extern AsyncMqttClient mqttClient;

String GetPayloadContent(char* data, size_t len)
{
        String content;
        content.reserve(len);
        for(size_t i = 0; i < len; i++)
        {
                content.concat(data[i]);
        }
        return content;
}

void SuscribeMqtt()
{
        uint16_t packetIdSub = mqttClient.subscribe(MQTT_TOPIC, 1);
        Serial.print("Subscribing to topic: ");
        Serial.print(MQTT_TOPIC);
        Serial.print(" at QoS 1, packetId: ");
        Serial.println(packetIdSub);
}

bool PublishMqtt(const String& payload)
{
        if(!mqttClient.connected())
        {
                return false;
        }

        mqttClient.publish(MQTT_TOPIC, 1, false, payload.c_str(), payload.length());
        return true;
}

void OnMqttReceived(char* topic,
                    char* payload,
                    AsyncMqttClientMessageProperties properties,
                    size_t len,
                    size_t index,
                    size_t total);
