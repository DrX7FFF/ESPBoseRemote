#include "PrivateWifi.h"
// #define WIFI_SSID "...."
// #define WIFI_PASSWD "...."

#define UART_BAUD 9600
#define PORT 9977

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>

WiFiServer server(PORT);
WiFiClient client;

void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  ArduinoOTA.begin();
  Serial.begin(UART_BAUD);
  server.begin();
}

int cnt = 0;

uint8_t buf1[1024];
uint8_t i1=0;
uint8_t buf2[1024];
uint8_t i2=0;

void loop() {
  ArduinoOTA.handle();
  delay(10);

  if(!client.connected()) { // if client not connected
    client = server.available(); // wait for it to connect
    return;
  }
  if(client.available()) {
    while(client.available()) {
      buf1[i1] = (uint8_t)client.read(); // read char from client (RoboRemo app)
      if(i1<1023) i1++;
    }
    // now send to UART:
    Serial.write(buf1, i1);
    i1 = 0;
  }
  if(Serial.available()) {
    while(Serial.available()) {
      buf2[i2] = (char)Serial.read(); // read char from UART
      if(i2<1023) i2++;
    }
    // now send to WiFi:
    client.write((char*)buf2, i2);
    i2 = 0;
    cnt = 0;
  }
  else {
    cnt++;
    if (cnt >= 100){
      cnt = 0;
      client.write("*", 1);
    }
  }
}
