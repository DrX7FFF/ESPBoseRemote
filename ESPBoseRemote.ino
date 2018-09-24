#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "PrivateWifi.h"
#include "Bose.h"
// #define WIFI_SSID "...."
// #define WIFI_PASSWD "...."

#define HOSTNAME "BOSEMON"

Bose bose;

void setup() {
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  //Init OTA
  ArduinoOTA.begin();
  bose.begin();
}

void loop() {
  ArduinoOTA.handle();

  bose.doloop();

  delay(10);
}

