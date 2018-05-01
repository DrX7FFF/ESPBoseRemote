#include "PrivateWifi.h"
// #define WIFI_SSID "...."
// #define WIFI_PASSWD "...."

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

  //Init OTA
  ArduinoOTA.begin();

  //Init Sérial
  Serial.setTimeout(300);   // 300ms de Timeout
  Serial.begin(19200);      // 19200 Standard Bose

  //Init TCP Serveur
  server.begin();
}

int cnt = 0;
uint8_t buf1[1024];
uint8_t i1=0;

uint8_t waitResponse = 0;


#define P_ACK       0x0100
#define P_VOLUME    0x0115
#define P_KEYEVENT  0x011b
#define P_READY     0x011d
#define P_KEYPRESS  0x0104

//Bose State
uint8_t boseLenError = 0;
uint8_t boseError = 0;
uint8_t boseVol = 0;
uint8_t boseKeyEvent = 0;
uint8_t boseSystemReady = 0;
uint8_t boseLastKeyCode = 0;
uint8_t boseLastKeyState = 0;

void loop() {
  ArduinoOTA.handle();
  delay(10);

  if(!client.connected()) { // if client not connected
    client = server.available(); // wait for it to connect
    if (client.connected())
      client.println("Bonjour");
  }
  // IP to Serial
  if(client.available() && waitResponse==0) {
    while(client.available()) {
      buf1[i1] = (uint8_t)client.read(); // read char from client (RoboRemo app)
      if(i1<1023) i1++;
    }
    // now send to UART:
    Serial.write(buf1, i1);
    i1 = 0;
    waitResponse = 1;
    client.println("Attente ACK");
  }
  
  // Serial to IP
  uint16_t ret = receiveFromBose();
  if (boseLenError){
    client.println("Erreur longeur de trame");
    boseLenError = 0;
  }
  else{
    if (boseError){
      client.print("Erreur sur le code :");
      client.println(ret,HEX);
      boseError = 0;
    }
    else{
      switch (ret){
        case P_ACK:
          waitResponse = 0;
          client.println("ACK");
          break;
        case P_READY:
          client.print("System Ready : ");
          client.println(boseSystemReady);
          break;
        case P_VOLUME:
          client.print("Volume : ");
          client.println(boseVol);
          break;
        case P_KEYEVENT:
          client.print("Evenement touche : ");
          client.println(boseKeyEvent);
          break;
        case P_KEYPRESS:
          client.print("Touche ");
          if (boseLastKeyState)
            client.print("relachée");
          else
            client.print("enfoncée");
          client.print(" code :");
          client.println(boseLastKeyCode,HEX);
          break;
        default:
          cnt++;
          if (cnt >= 1000){
            cnt = 0;
            client.write(".", 1);
          }
      }
    }
  }
}

#define MAXPACKETSIZE 100
uint8_t buf[MAXPACKETSIZE];

uint16_t receiveFromBose(){
  uint8_t len;
  char temp;
  size_t readlen;
  
  if(!Serial.available())
    return 0;

  temp = Serial.read();
  len = (uint8_t)temp - 1;
  if (len>MAXPACKETSIZE) len=MAXPACKETSIZE;

  readlen = Serial.readBytes(buf,len);
  //Debug
//  client.write(temp);
//  client.write(buf,readlen);
  
  if (readlen != len) {
    boseLenError = 1;
    return 0; //Pas reçu toute la longeur Exit
  }
  return decodResponse();    
}

uint16_t decodResponse(){
  uint16_t opcode = buf[1]<<8 | buf[2];
  // Cas d'erreur
  if (buf[0] & 0x80){
    boseError = 1;
  }  
  else{
    if (buf[0] & 0x08){  // cas d'une notification
      // Détection de touche
      if (opcode == P_KEYPRESS){
        boseLastKeyState = buf[3];
        boseLastKeyCode = buf[5];
      }
    }
    else {                // cas Response
      switch (opcode){
        case P_VOLUME:      // Retour de Volume
          boseVol = buf[3];
          break;
        case P_KEYEVENT:    // Retour de notification KeyPresse
          boseKeyEvent = buf[3];
          break;
        case P_READY:       // Retour System Ready
          boseSystemReady = buf[3];
          break;
      }
    }
  }
  return opcode;
}
