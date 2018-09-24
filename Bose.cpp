#include "Bose.h"
#define DEBUG_PORT  9977
#define DISP_PORT   9978

Bose::Bose(){
//  dispServer(DISP_PORT);
//  debugServer(DEBUG_PORT);
//  dispServer = new WiFiServer(DISP_PORT);
//  debugServer = new WiFiServer(DEBUG_PORT);
}

void Bose::begin(){
    //Init Sérial
  Serial.setTimeout(300);   // 300ms de Timeout
  Serial.begin(19200);      // 19200 Standard Bose

  //Init TCP Serveur
  debugServer->begin();
  dispServer->begin();
}

void Bose::doloop(){
  if(!debugClient.connected()) { // if client not connected
    debugClient = debugServer->available(); // wait for it to connect
    if (debugClient.connected()){
      debugClient.println("Debug connected");
      if (dispClient.connected())
        debugClient.println("Client connected");
    }
  }

  // TESTER la déconnexion et reconnexion WIFI

  if(!dispClient.connected()) { // if client not connected
    dispClient= dispServer->available(); // wait for it to connect
    if (dispClient.connected()){
      dispClient.println(boseOff?"Bose Off":"Bose On");
      if (debugClient.connected())
        debugClient.println("Client connected");
    }
  }

  // A FAIRE
  // logger les timeouts consécutif et mettre en erreur
  // Séquence de suveillance du Bose
  if (waitACK){
    waitACK--;
    // si waitACK == 0 alors TimeOut donc on libère
    if (waitACK == 0){
      Serial.flush();
      while(Serial.available () > 0)
        Serial.read();
    }
  }
  else{
    if (ctrlVolumeAct){
      if (ctrlVolumeAct==CTRLVOL_COUNT)
        sendBose(cmdGetVolume,sizeof(cmdGetVolume));
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
            // Vie pour le display en début de control
            dispClient.print(".");
            if (!boseKeyEvent){
              sendBose(cmdEnableKeyEvent,sizeof(cmdEnableKeyEvent));
              break;
            }
        case 1:
          seqIndex++;
          sendBose(cmdGetKeyEvent,sizeof(cmdGetKeyEvent));
          break;
        case 2:
          seqIndex++;          
          sendBose(cmdGetRmState,sizeof(cmdGetRmState));
          break;
        case 3:
          seqIndex++;
          sendBose(cmdGetVolume,sizeof(cmdGetVolume));
          break;
        case 4:
          seqCnt = SEQ_COUNT;
        default:
          seqIndex=0;
      }
  }
  receiveBose();

    // traitement sur retour de touche
  if (ctrlVolume && ctrlVolumeAct == 0)
    ctrlVolumeAct = CTRLVOL_COUNT;
}


void Bose::debugPrintHex(char c){
  if (c<0x10)
    streamRef->print("0");    
  streamRef->print(c, HEX);
  streamRef->print(" ");
}

void Bose::setTimeOut(const uint8_t value){
  if (timeOut != value)
    timeOut = value;
}
void Bose::setMute(const uint8_t value){
  if (boseMute != value){
    boseMute = value;
    notifyVolume();
  }
}
void Bose::setOff(const uint8_t value){
  if (boseOff != value){
    boseOff = value;
    notifyPower();
  }
}
void Bose::setVolume(const uint8_t value){
  if (boseVol != value){
    boseVol = value;
    notifyVolume();
  }
}
void Bose::setSource(const uint8_t value){
  if (boseSource != value){
    boseSource = value;
    notifySource();
  }
}

void Bose::notifyPower(){  
//    dispClient.println(boseOff?"Bose Off":"Bose On");
}
void Bose::notifyVolume(){
//    dispClient.print("Volume :");
//    if (boseMute)
//      dispClient.println("Mute");
//    else
//      dispClient.println(boseVol);
}
void Bose::notifySource(){
//  dispClient.print("Source :");
//  switch (boseSource){
//    case 0x0E:
//      dispClient.println("TV");
//      break;
//    case 0x11:
//      dispClient.println("Menu");
//      break;
//  }
}

void Bose::sendBose(const uint8_t* msg, uint8_t len){
  // interdiction de l'envoi si attente d'un ACK
  if (waitACK)
    return;

  //Debug
  streamRef->print("S: ");    
  for(uint8_t i=0; i< len; i++)
    debugPrintHex(msg[i]);
  streamRef->println();
  
  // now send to UART:
  Serial.write(msg, len);
  waitACK = ACK_COUNT;
}

void Bose::receiveBose(){
  uint8_t len;
  size_t readlen;
  
  if(!Serial.available())
    return;

  len = Serial.read();
  if (len>MAXPACKETSIZE) len=MAXPACKETSIZE;
  readlen = Serial.readBytes(buf,len-1);

  //Debug
  streamRef->print("R: ");
  debugPrintHex(len);
  for(uint8_t i=0; i< readlen; i++)
    debugPrintHex(buf[i]);
  streamRef->println();
  
  if ((readlen+1) != len) {
    streamRef->println("Erreur de longueur");
    return; //Pas reçu toute la longeur Exit
  }
  decodResponse();
}

void Bose::decodResponse(){
  uint16_t opcode = buf[1]<<8 | buf[2];
  // Cas d'erreur
  if (buf[0] & 0x80){
    if(streamRef)
      streamRef->println("Bose return ERROR");
    boseOnError = 1;
    return;
  }
  
  if (buf[0] & 0x08){  // cas d'une notification
    // Détection de touche
    if (opcode == 0x0104)
      decodKey(buf[5], buf[3]);
  }
  else {                // cas Response
    switch (opcode){
      case 0x0100:         // Ack
        waitACK = 0;
        break;
      case 0x010A:      // Retour source
        setSource(buf[3]);
        break;
      case 0x0111:      // Retour Room State
        setMute(buf[4]);
        setOff(buf[5]);
        break;
      case 0x0115:      // Retour de Volume
        setVolume(buf[3]);
        break;
      case 0x011b:    // Retour de notification KeyPresse
        boseKeyEvent = buf[3];
        break;
    }
  }
}

void Bose::decodKey(uint8_t keyCode, uint8_t keyState){
  streamRef->print("Touche ");
  debugPrintHex(keyCode);
  streamRef->println(keyState?"relachee":"enfoncee");

  ctrlRmState = 0;
  ctrlVolume = 0;
  ctrlSource = 0;
  if (!keyState)
  switch (keyCode){
    case 0x01: //mute
    case 0x4C: //Power
      ctrlRmState = 1;
      break;
    case 0x02: //Vol-
    case 0x03: //Vol+
      ctrlVolume = 1;
      break;
    case 0xD3: //Source
      ctrlSource = 1;
      break;
  }
}
