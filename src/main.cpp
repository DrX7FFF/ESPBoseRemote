#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
// #include "PrivateWifi.h"
// #define WIFI_SSID "...."
// #define WIFI_PASSWD "...."


#define DEBUG_PORT 9977
#define DISP_PORT 9978
#define HOSTNAME "BOSEMON"

WiFiServer dispServer(DISP_PORT);
WiFiClient dispClient;
unsigned long dispClientMillis;
#define DISPCLIENTWATCHDOG 20000

WiFiServer debugServer(DEBUG_PORT);
WiFiClient debugClient;

#include "SerialBose.h"

void setup() {
  WiFi.hostname(HOSTNAME);
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  //Init OTA
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.begin();

  //Init Sérial
  Serial.setTimeout(300);   // 300ms de Timeout
  Serial.begin(19200);      // 19200 Standard Bose

  //Init TCP Serveur
  debugServer.begin();
  dispServer.begin();

/*
  // Attente du debug avant de continuer
  while(!debugClient.connected()) { // if client not connected
    debugClient = debugServer.available(); // wait for it to connect
    ArduinoOTA.handle();
    delay(100);
  }
  debugClient.println("Start 3");
  */
}

//uint8_t i = 0;
uint8_t buf1[1024];
uint8_t i1=0;


//Bose State

const uint8_t cmdEnableKeyEvent[]   = {0x07, 0x00, 0x01, 0x1b, 0x01, 0x0d, 0x11};
const uint8_t cmdGetKeyEvent[]      = {0x07, 0x00, 0x01, 0x1b, 0x00, 0x00, 0x1d};
//const uint8_t cmdGetPower[]         = {0x07, 0x00, 0x01, 0x1d, 0x00, 0x00, 0x1b};
const uint8_t cmdGetVolume[]        = {0x07, 0x00, 0x01, 0x15, 0x00, 0x1e, 0x0d};
const uint8_t cmdGetRoomState[]       = {0x08, 0x00, 0x01, 0x11, 0x00, 0x02, 0x00, 0x1A};
const uint8_t cmdGetSource[]        = {0x08, 0x00, 0x01, 0x0A, 0x00, 0x18, 0x18, 0x03};


// Init sur 
// retour de WIFI
// retour defaut de COM
// activation Bose

void loop() {
  ArduinoOTA.handle();

  if(!debugClient.connected()) { // if client not connected
    debugClient = debugServer.available(); // wait for it to connect
    if (debugClient.connected()){
      debugClient.println("Debug connected");
      if (dispClient.connected())
        debugClient.println("Client connected");
    }
  }

  // A FAIRE TESTER la déconnexion et reconnexion WIFI

  if(!dispClient.connected()) { // if client not connected
    dispClient= dispServer.available(); // wait for it to connect
    if (dispClient.connected()){
      dispClient.setNoDelay(true);
      sendToDisplay();
      if (debugClient.connected())
        debugClient.println("Client connected");
    }
  }

  // Ecoute du Bose
  receiveFromBose();

  // A FAIRE
  // logger les timeouts consécutif et mettre en erreur
  // Séquence de suveillance du Bose
  if (waitACK){
    waitACK--;
    // si waitACK == 0 alors TimeOut donc on libère
    if (waitACK == 0){
      debugClient.println("Time out");
      setBoseReady(0);
    }
  }
  else{
    if (boseReady == 0){
      debugClient.println("Init");
      // Purge du port série
      Serial.flush();
      while(Serial.available () > 0)
        Serial.read();
      
      boseKeyEvent = 0;
      ctrlRoomState = 1;
      sendToBose(cmdGetKeyEvent,sizeof(cmdGetKeyEvent));
    }
    else if (boseKeyEvent ==0) {
      debugClient.println("Activation Event");
      sendToBose(cmdEnableKeyEvent,sizeof(cmdEnableKeyEvent));
      setBoseReady(0);
    }
    else if (ctrlRoomState){
      debugClient.println("Ctrl RoomState");
      ctrlRoomState = 0;
      sendToBose(cmdGetRoomState,sizeof(cmdGetRoomState));      
    }
    else if (ctrlRoomStateScan){
      if ((ctrlRoomStateScan & 0x0F) == 0x01){
        debugPrintHex(ctrlRoomStateScan);
        debugClient.print(" Ctrl RoomState Scan");
        sendToBose(cmdGetRoomState,sizeof(cmdGetRoomState));
      }
      ctrlRoomStateScan--;
    }
    else if (ctrlVolume){
      debugClient.println("Ctrl Volume");
      ctrlVolume = 0;
      sendToBose(cmdGetVolume,sizeof(cmdGetVolume));      
    }
    else if (ctrlVolumePress){
      debugClient.println("Ctrl Volume Press");
      sendToBose(cmdGetVolume,sizeof(cmdGetVolume));
    }
    else{
      if (waitState==0){
        waitState = WAITSTATE_INIT;
        if (boseOff){
          debugClient.println("Surveillance KeyEvent");
          sendToBose(cmdGetKeyEvent,sizeof(cmdGetKeyEvent));
        }
        else{
          debugClient.println("Surveillance Source");
          sendToBose(cmdGetSource,sizeof(cmdGetSource));
        }
      }
      waitState--;
    }
  }

  // Debug String to Bose
  if(debugClient.connected()) {
    while(debugClient.available()) {
      buf1[i1] = (uint8_t)debugClient.read();
      if(i1<1023) i1++;
    }
    if (i1>0){
      sendToBose(buf1,i1);
      i1 = 0;
    }
  }

  if ( millis() - dispClientMillis > DISPCLIENTWATCHDOG)
    sendToDisplay();
  
  delay(10);
}
