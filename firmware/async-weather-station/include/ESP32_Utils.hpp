#pragma once
#include <WiFi.h>

void ConnectWiFi_STA()
{
   Serial.println("");
   WiFi.mode(WIFI_STA);
   WiFi.setHostname(hostname);
   WiFi.begin(ssid, password);
   // Eliminado: uso de IP estática (WiFi.config), ahora DHCP asigna la dirección automáticamente.
   while (WiFi.status() != WL_CONNECTED)
   {
     delay(100);
     Serial.print('.');
   }
 
   Serial.println("");
   Serial.print("Iniciado STA:\t");
   Serial.println(ssid);
   Serial.print("IP address:\t");
   Serial.println(WiFi.localIP());
}

void ConnectWiFi_AP()
{
   Serial.println("");
   WiFi.mode(WIFI_AP);
   while(!WiFi.softAP(ssid, password))
   {
     Serial.println(".");
     delay(100);
   }
   // Eliminado: configuración manual de IP en modo AP, se mantiene la asignación por defecto del ESP32.

   Serial.println("");
   Serial.print("Iniciado AP:\t");
   Serial.println(ssid);
   Serial.print("IP address:\t");
   Serial.println(WiFi.softAPIP());
}
