#pragma once

#include <Arduino.h>

#include "ESP32_Utils.hpp"
#include "MQTT.hpp"

inline void InitMqtt()
{
        ConfigureMqttClient();
}

inline void EnsureMqttConnection()
{
        if(mqttClient.connected())
        {
                return;
        }

        while(!ConnectMqtt())
        {
                Serial.println("Intentando de nuevo en 5 segundos");
                delay(5000);
        }
}

inline void HandleMqttLoop()
{
        EnsureWiFiConnected();
        EnsureMqttConnection();
        mqttClient.loop();
}
