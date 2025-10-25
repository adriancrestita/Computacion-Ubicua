#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

inline void PrintLocalIp()
{
        Serial.print("Dirección IP: ");
        Serial.println(WiFi.localIP());
}

inline void ConnectWiFi_STA(bool waitForConnection = true)
{
        Serial.println();
        Serial.print("Conectando a ");
        Serial.println(WIFI_SSID);

        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        if(!waitForConnection)
        {
                return;
        }

        while(WiFi.status() != WL_CONNECTED)
        {
                delay(500);
                Serial.print('.');
        }

        Serial.println();
        Serial.println("WiFi conectado");
        PrintLocalIp();
}

inline bool EnsureWiFiConnected(unsigned long retryDelayMs = 5000)
{
        if(WiFi.status() == WL_CONNECTED)
        {
                return true;
        }

        Serial.println("WiFi desconectado, intentando reconectar...");
        WiFi.reconnect();

        unsigned long start = millis();
        while(WiFi.status() != WL_CONNECTED && millis() - start < retryDelayMs)
        {
                delay(500);
                Serial.print('.');
        }

        Serial.println();

        if(WiFi.status() == WL_CONNECTED)
        {
                Serial.println("WiFi reconectado correctamente");
                PrintLocalIp();
                return true;
        }

        Serial.println("No se pudo reconectar al WiFi.");
        return false;
}
