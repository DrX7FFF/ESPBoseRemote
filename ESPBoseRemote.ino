#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include "PrivateWifi.h"
// #define WIFI_SSID "...."
// #define WIFI_PASSWD "...."

#define DEBUG_PORT 9977
#define DISP_PORT 9978
#define HOSTNAME "BOSEMON"

WiFiServer dispServer(DISP_PORT);
WiFiClient dispClient;

WiFiServer debugServer(DEBUG_PORT);
WiFiClient debugClient;

void setup() {
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
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

  // Attente du debug avant de continuer
  while(!debugClient.connected()) { // if client not connected
    debugClient = debugServer.available(); // wait for it to connect
    ArduinoOTA.handle();
    delay(100);
  }
  debugClient.println("Start 3");
}

uint8_t i = 0;
uint8_t buf1[1024];
uint8_t i1=0;

// 250x10ms = 2.5 s
#define WAITACK_INIT 0xFA
uint8_t waitACK = 0;

// 250x10ms = 2.5 s
#define WAITSTATE_INIT 0x1FA
uint16_t waitState = WAITSTATE_INIT;
#define CTRLVOL_COUNT 6

//Bose State
uint8_t boseOnError = 0;
// uint8_t bosePower = 0;
uint8_t boseOff = 0x03; //Eteint par défaut
uint8_t boseVol = 0;
uint8_t boseMute = 0;
uint8_t boseSource = 0;
uint8_t boseKeyEvent = 0;

uint8_t initMode = 1;
uint8_t ctrlVolume = 0;
//uint8_t ctrlVolumeAct = 0;
uint8_t ctrlPower = 0;
uint8_t ctrlSource = 0;
uint8_t ctrlPowerOff = 0;
#define WAITPOWEROFF_INIT 0xF0
uint8_t ctrlVolumePress = 0;

const uint8_t cmdEnableKeyEvent[]   = {0x07, 0x00, 0x01, 0x1b, 0x01, 0x0d, 0x11};
const uint8_t cmdGetKeyEvent[]      = {0x07, 0x00, 0x01, 0x1b, 0x00, 0x00, 0x1d};
//const uint8_t cmdGetPower[]         = {0x07, 0x00, 0x01, 0x1d, 0x00, 0x00, 0x1b};
const uint8_t cmdGetVolume[]        = {0x07, 0x00, 0x01, 0x15, 0x00, 0x1e, 0x0d};
const uint8_t cmdGetRmState[]       = {0x08, 0x00, 0x01, 0x11, 0x00, 0x02, 0x00, 0x1A};
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
      initMode = 1;
      Serial.flush();
      while(Serial.available () > 0)
        Serial.read();
    }
  }
  else{
    if (initMode){
      debugClient.println("Init");
      initMode = 0;
      boseKeyEvent = 0;
      boseOnError = 0;
      ctrlVolume = 1;
      ctrlPower = 1;
      ctrlSource = 1;
      sendToBose(cmdGetKeyEvent,sizeof(cmdGetKeyEvent));
    }
    else if (boseKeyEvent ==0) {
      debugClient.println("Activation Event");
      initMode = 1;
      sendToBose(cmdEnableKeyEvent,sizeof(cmdEnableKeyEvent));
    }
    else if (ctrlPower){
      debugClient.println("Ctrl Power");
      ctrlPower = 0;
      sendToBose(cmdGetRmState,sizeof(cmdGetRmState));      
    }
    else if (ctrlPowerOff){
      ctrlPowerOff--;
      if ((ctrlPowerOff & 0x0F) == 0x01){
        debugClient.print("Ctrl Power Off ");
        debugPrintHex(ctrlPowerOff);
        sendToBose(cmdGetRmState,sizeof(cmdGetRmState));
      }
    }
    else if (ctrlSource){
      debugClient.println("Ctrl Source");
      ctrlSource = 0;
      sendToBose(cmdGetSource,sizeof(cmdGetSource));      
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
        /* En commentaire pour les tests
        debugClient.println("Surveillance");
        sendToBose(cmdGetKeyEvent,sizeof(cmdGetKeyEvent));
        */
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
  
  // traitement sur retour de touche
  // if (ctrlVolume && ctrlVolumeAct == 0)
    // ctrlVolumeAct = CTRLVOL_COUNT;

  delay(10);
}

void sendToDisplay(){  
  dispClient.print("Err:");
  dispClient.print(boseOnError?"On ":"Off");
  
  dispClient.print(" Evt:");
  dispClient.print(boseKeyEvent?"On ":"Off");
  
  dispClient.print(" Pwr:");  
  dispClient.print(boseOff?"Off":"On ");
  
  dispClient.print(" Mut:");
  dispClient.print(boseMute?"On ":"Off");
  
  dispClient.print(" Vol:");
  dispClient.print(boseVol);

  dispClient.print(" Src:");
  switch (boseSource){
    case 0x00:
      dispClient.println("Off");
      break;
    case 0x0E:
      dispClient.println("TV");
      break;
    case 0x11:
      dispClient.println("Menu");
      break;
    default:
      dispClient.println("???");
      break;
  }
}

void sendToBose(const uint8_t* msg, uint8_t len){
  // interdiction de l'envoi si attente d'un ACK
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
  waitACK = WAITACK_INIT;
}

// 20 caractètes sont suffisant par rapport au 255 théorique
#define MAXPACKETSIZE 20
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
    if(debugClient.connected())
      debugClient.println("Erreur de longueur");
    boseOnError = 1;
    return; //Pas reçu toute la longeur Exit
  }
//  boseOnError = 0;
  decodResponse();  

  // Reset control de vie
  waitState = WAITSTATE_INIT;
}

void decodResponse(){
  uint16_t opcode = buf[1]<<8 | buf[2];
  uint8_t sendToDisplayToDo = 0;
  // Cas d'erreur
  if (buf[0] & 0x80){
    if(debugClient.connected())
      debugClient.println("Bose return ERROR");
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
      case 0x010A:      // Retour source
        if (boseSource != buf[5]){ // Test du 5 plutot que le 3
          boseSource = buf[5];
          dispClient.print("[Src] ");
          sendToDisplayToDo = 1;
        }
        break;
      case 0x0111:      // Retour Room State
        if (boseOff != buf[5]){
          boseOff = buf[5];
          dispClient.print("[Pwr] ");
          sendToDisplayToDo = 1;
          if (boseOff == 0) // Si devient actif alors controler la source
            ctrlSource = 1;
          else{
            if (ctrlPowerOff) //si devient inactif alors arrête surveillance arrêt
              ctrlPowerOff = 0;          
          }
        }
        if (boseMute != buf[4]){
          boseMute = buf[4];
          dispClient.print("[Mut] ");
          sendToDisplayToDo = 1;
        }
        break;
      case 0x0115:      // Retour de Volume
        if (boseVol != buf[3]){
          boseVol = buf[3];
          dispClient.print("[Vol] ");
          sendToDisplayToDo = 1;
        }
        break;
      case 0x011b:    // Retour de etat KeyPresseEvent
        if (boseKeyEvent != buf[3])
          boseKeyEvent = buf[3];
        break;
      // case 0x011d:       // Retour System Ready
        // if (bosePower != buf[3]){
          // bosePower = buf[3];
          // dispClient.println(bosePower == 0?"Bose Off":"Bose On");
        // }
        // break;
    }
    if (sendToDisplayToDo)
      sendToDisplay();
  }
}

void decodKey(uint8_t keyCode, uint8_t keyState){
  if(debugClient.connected()){
    debugClient.print("Touche ");
    debugPrintHex(keyCode);
    debugClient.println(keyState?"relachee":"enfoncee");
  }
  switch (keyCode){
    case 0x01: //mute
      if (keyState !=0) // si relaché vérifier room state
        ctrlPower = 1;
      break;
    case 0x02: //Vol-
    case 0x03: //Vol+
      if (keyState !=0){ // si relaché vérifier une dernière fois room state
        ctrlVolume = 1;
        ctrlVolumePress = 0;
      }
      else{   //si enfoncé en surveille le room state
        ctrlVolume = 0;
        ctrlVolumePress = 1;
      }
      break;
    case 0x30: //Exit : dans le cas ou on quitte le menu
    case 0xD3: //Source
      if (keyState !=0) // si relaché vérifier room state
        ctrlSource = 1;
      break;
    case 0x4C: //Power
      if (keyState !=0){ // si relaché vérifier room state
        if (boseOff)
          ctrlPower = 1;
        else   
          ctrlPowerOff = WAITPOWEROFF_INIT;
      }
      break;
  }
}
void debugPrintHex(char c){
  if (c<0x10)
    debugClient.print("0");    
  debugClient.print(c, HEX);
  debugClient.print(" ");
}
