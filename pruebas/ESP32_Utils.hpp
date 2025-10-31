#pragma once
#include <WiFi.h>
#include <IPAddress.h>

// Este archivo ASUME que las siguientes macros están definidas en el archivo de configuración incluido por el sketch:
// WIFI_SSID, WIFI_PASSWORD
// WIFI_IP, WIFI_GATEWAY, WIFI_SUBNET (si se usa IP estática)

void ConnectWiFi_STA(bool useStaticIP = false)
{
   Serial.println("");
   // Usamos las macros definidas en el archivo de configuración
   WiFi.mode(WIFI_STA);
   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
   
   if(useStaticIP) 
   {
      // Las macros de IP deben definirse con comas (ej: 192,168,1,200)
      IPAddress ip(WIFI_IP);
      IPAddress gateway(WIFI_GATEWAY);
      IPAddress subnet(WIFI_SUBNET);
      WiFi.config(ip, gateway, subnet);
   }
   
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

void ConnectWiFi_AP(bool useStaticIP = false)
{ 
   Serial.println("");
   WiFi.mode(WIFI_AP);
   // Usamos las macros definidas en el archivo de configuración
   while(!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD))
   {
     Serial.println(".");
     delay(100);
   }
   
   if(useStaticIP)
   {
      IPAddress ip(WIFI_IP);
      IPAddress gateway(WIFI_GATEWAY);
      IPAddress subnet(WIFI_SUBNET);
      WiFi.softAPConfig(ip, gateway, subnet);
   }

   Serial.println("");
   Serial.print("Iniciado AP:\t");
   Serial.println(WIFI_SSID);
   Serial.print("IP address:\t");
   Serial.println(WiFi.softAPIP());
}