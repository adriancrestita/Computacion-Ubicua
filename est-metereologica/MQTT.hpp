#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <cstring>

#include "config.h"

static WiFiClient mqttWifiClient;
static PubSubClient mqttClient(mqttWifiClient);

inline String BuildClientId()
{
        if(strlen(MQTT_CLIENT_ID) > 0)
        {
                return MQTT_CLIENT_ID;
        }

        String clientId = "ESP32Client-";
        clientId += String(millis(), HEX);
        return clientId;
}

inline void ConfigureMqttClient()
{
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
}

inline void SetMqttCallback(MQTT_CALLBACK_SIGNATURE)
{
        mqttClient.setCallback(callback);
}

inline void SubscribeDefaultTopic()
{
        if(strlen(MQTT_TOPIC_SUB) == 0)
        {
                return;
        }

        mqttClient.subscribe(MQTT_TOPIC_SUB);
}

inline bool ConnectMqtt()
{
        Serial.print("Intentando conexión MQTT...");

        String clientId = BuildClientId();
        bool connected = false;

        if(strlen(MQTT_USER) > 0)
        {
                connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
        }
        else
        {
                connected = mqttClient.connect(clientId.c_str());
        }

        if(connected)
        {
                Serial.println("conectado");
                SubscribeDefaultTopic();
        }
        else
        {
                Serial.print("falló, rc=");
                Serial.println(mqttClient.state());
        }

        return connected;
}

inline bool PublishMqttMessage(const char* topic, const String& payload, bool retained = false)
{
        if(topic == nullptr || payload.length() == 0)
        {
                return false;
        }

        return mqttClient.publish(topic, payload.c_str(), retained);
}

inline bool PublishDefaultTopic(const String& payload, bool retained = false)
{
        if(strlen(MQTT_TOPIC_PUB) == 0)
        {
                return false;
        }

        return PublishMqttMessage(MQTT_TOPIC_PUB, payload, retained);
}
