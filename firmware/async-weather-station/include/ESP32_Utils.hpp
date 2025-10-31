#pragma once
#include <WiFi.h>
// Este archivo ASUME que las siguientes macros están definidas en el archivo de configuración incluido por el sketch:
// WIFI_SSID, WIFI_PASSWORD

void ConnectWiFi_STA()
{
   Serial.println("");
   // Usamos las macros definidas en el archivo de configuración
   WiFi.mode(WIFI_STA);
   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
   
   while (WiFi.status() != WL_CONNECTED)
   { 
     delay(100);  
     Serial.print('.'); 
   }
 
   Serial.println("");
   Serial.print("Iniciado STA:\t");
   Serial.println(WIFI_SSID);
   Serial.print("IP address:\t");
   Serial.println(WiFi.localIP());
}

void ConnectWiFi_AP()
{ 
   Serial.println("");
   WiFi.mode(WIFI_AP);
   // Usamos las macros definidas en el archivo de configuración
   while(!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD))
   {
     Serial.println(".");
     delay(100);
   }
   
   Serial.println("");
   Serial.print("Iniciado AP:\t");
   Serial.println(WIFI_SSID);
   Serial.print("IP address:\t");
   Serial.println(WiFi.softAPIP());
}