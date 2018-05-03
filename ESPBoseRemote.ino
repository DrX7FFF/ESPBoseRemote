#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include "PrivateWifi.h"
// #define WIFI_SSID "...."
// #define WIFI_PASSWD "...."

#define DEBUG_PORT 9977
#define DISP_PORT 9978

WiFiServer dispServer(DISP_PORT);
WiFiClient dispClient;

WiFiServer debugServer(DEBUG_PORT);
WiFiClient debugClient;

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
  debugServer.begin();
  dispServer.begin();
}

uint8_t i = 0;
uint8_t buf1[1024];
uint8_t i1=0;

// 250x10ms = 2.5 s
#define ACK_COUNT 0xFA
uint8_t waitACK = 0;

// 0x3E8 1000*10ms = 10s
#define SEQ_COUNT 0xBB8 
uint16_t seqCnt = 0;
uint8_t seqIndex = 0;

#define CTRLVOL_COUNT 6

//Bose State
uint8_t boseOnError = 0;
uint8_t bosePower = 0;
uint8_t boseVol = 0;
uint8_t boseKeyEvent = 0;

uint8_t ctrlVolume = 0;
uint8_t ctrlVolumeAct = 0;

uint8_t ctrlPower = 0;

const uint8_t cmdEnableKeyEvent[]   = {0x07, 0x00, 0x01, 0x1b, 0x01, 0x0d, 0x11};
const uint8_t cmdGetKeyEvent[]      = {0x07, 0x00, 0x01, 0x1b, 0x00, 0x00, 0x1d};
const uint8_t cmdGetPower[]         = {0x07, 0x00, 0x01, 0x1d, 0x00, 0x00, 0x1b};
const uint8_t cmdGetVolume[]        = {0x07, 0x00, 0x01, 0x15, 0x00, 0x1e, 0x0d};

void loop() {
  ArduinoOTA.handle();
  delay(10);

  if(!debugClient.connected()) { // if client not connected
    debugClient = debugServer.available(); // wait for it to connect
    if (debugClient.connected()){
      debugPrintln("Debug connected");
      if (dispClient.connected())
        debugPrintln("Client connected");
    }
  }

  if(!dispClient.connected()) { // if client not connected
    dispClient= dispServer.available(); // wait for it to connect
    if (dispClient.connected()){
      dispClient.println(bosePower == 0?"Bose Off":"Bose On");
      debugPrintln("Client connected");
    }
  }

  // Mettre en place un flush du serial si erreur

  // TEMP DEBUG
//  if (!debugClient.connected())
//    return;
    
  // Séquence de suveillance du Bose
  if (waitACK){
    debugClient.print("#");
    waitACK--;
  }
  else{
    if (ctrlVolumeAct){
      if (ctrlVolumeAct==CTRLVOL_COUNT)
        sendToBose(cmdGetVolume,7);
      if (ctrlVolume)
        ctrlVolumeAct--;
      else
        ctrlVolumeAct=0;
      seqCnt = SEQ_COUNT;
    }
    
    if (seqCnt)
      seqCnt--;
    else
      switch (seqIndex){
        case 0:
            seqIndex++;
            if (!boseKeyEvent){
              sendToBose(cmdEnableKeyEvent,7);
              break;
            }
        case 1:
          seqIndex++;
          sendToBose(cmdGetKeyEvent,7);
          break;
        case 2:
          seqIndex++;
          sendToBose(cmdGetPower,7);
          break;
        case 3:
          seqIndex++;
          sendToBose(cmdGetVolume,7);
          break;
        case 4:
          dispClient.print(".");
          seqCnt = SEQ_COUNT;
        default:
          seqIndex=0;
      }
  }

  // IP to Serial
  if(debugClient.available()) {
    while(debugClient.available()) {
      buf1[i1] = (uint8_t)debugClient.read();
      if(i1<1023) i1++;
    }
    sendToBose(buf1,i1);
    i1 = 0;
  }

  // Serial to IP
  receiveFromBose();

  if (ctrlVolume && ctrlVolumeAct == 0)
    ctrlVolumeAct = CTRLVOL_COUNT;

}

void sendToBose(const uint8_t* msg, const uint8_t len){
  if (waitACK)
    return;

  if(debugClient.connected()) {
    debugClient.print("S: ");
    for(i=0; i< len; i++)
      debugPrintHex(msg[i]);
    debugClient.println();
  }
    // now send to UART:
  Serial.write(msg, len);
  waitACK = ACK_COUNT;
}

#define MAXPACKETSIZE 100
uint8_t buf[MAXPACKETSIZE];

void receiveFromBose(){
  uint8_t len;
  size_t readlen;
  
  if(!Serial.available())
    return;

  len = Serial.read();
  if (len>MAXPACKETSIZE) len=MAXPACKETSIZE;
  readlen = Serial.readBytes(buf,len-1);

  //Debug
  if(debugClient.connected()) {
    debugClient.print("R: ");
    debugPrintHex(len);
    for(i=0; i< readlen; i++)
      debugPrintHex(buf[i]);
    debugClient.println();
  }
  
  if ((readlen+1) != len) {
    debugPrint("Longueur attendu ");
    debugPrintHex(len);
    debugPrint("recu ");
    debugPrintlnHex(readlen+1);
    boseOnError = 1;
    return; //Pas reçu toute la longeur Exit
  }
  decodResponse();  
}

void decodResponse(){
  uint16_t opcode = buf[1]<<8 | buf[2];
  // Cas d'erreur
  if (buf[0] & 0x80){
    debugPrint("Bose return ERROR");
    boseOnError = 1;
    return;
  }
  
  if (buf[0] & 0x08){  // cas d'une notification
    // Détection de touche
    if (opcode == 0x0104){
      decodKey(buf[5], buf[3]);
    }
  }
  else {                // cas Response
    switch (opcode){
      case 0x0100:         // Ack
        waitACK = 0;
        break;
      case 0x0115:      // Retour de Volume
        if (boseVol != buf[3]){
          boseVol = buf[3];
          dispClient.print("Volume :");
          dispClient.println(boseVol);
        }
        break;
      case 0x011b:    // Retour de notification KeyPresse
        boseKeyEvent = buf[3];
        break;
      case 0x011d:       // Retour System Ready
        if (bosePower != buf[3]){
          bosePower = buf[3];
          dispClient.println(bosePower == 0?"Bose Off":"Bose On");
        }
        break;
    }
  }
}

void decodKey(uint8_t keyCode, uint8_t keyState){
  debugPrint("Touche ");
  debugPrintHex(keyCode);
  if (keyState)
    debugPrintln("relachée");
  else
    debugPrintln("enfoncée");
  switch (keyCode){
    case 0x01: //mute
      break;
    case 0x02: //Vol-
    case 0x03: //Vol+
      ctrlVolume = !keyState;
      break;
    case 0xD3: //Source
      break;
    case 0x4C: //Power
      ctrlPower = !keyState;
      break;
  }
}

void debugPrintln(char* m){
    if(debugClient.connected())
      debugClient.println(m);
}

void debugPrint(char* m){
    if(debugClient.connected())
      debugClient.print(m);
}

void debugPrintlnHex(char c){
  if (c<0x10)
    debugClient.print("0");    
  debugClient.println(c, HEX);
}

void debugPrintHex(char c){
  if (c<0x10)
    debugClient.print("0");    
  debugClient.print(c, HEX);
  debugClient.print(" ");
}
