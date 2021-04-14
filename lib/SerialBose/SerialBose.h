// 20 caractètes sont suffisant par rapport au 255 théorique
#define MAXPACKETSIZE 20
uint8_t buf[MAXPACKETSIZE];

//Bose State
uint8_t boseReady = 0;
// uint8_t bosePower = 0;
uint8_t boseOff = 0x03; //Eteint par défaut
uint8_t boseVol = 0;
uint8_t boseMute = 0;
uint8_t boseSource = 0;
uint8_t boseKeyEvent = 0;

uint8_t ctrlVolume = 0;
//uint8_t ctrlVolumeAct = 0;
uint8_t ctrlRoomState = 0;
uint8_t ctrlRoomStateScan = 0;
#define SCAN_INIT 0xF0
uint8_t ctrlVolumePress = 0;

// 250x10ms = 2.5 s
#define WAITACK_INIT 0xFA
uint8_t waitACK = 0;

// 250x10ms = 2.5 s
#define WAITSTATE_INIT 0x1FA
uint16_t waitState = WAITSTATE_INIT;
#define CTRLVOL_COUNT 6

void sendToDisplay(){
  uint8_t sendBuf[3] = {0x80, 0x00, 0x00};
  sendBuf[1] = ((boseReady ==0) | (boseSource==0x11))<<1 | (boseOff==0);
//  sendBuf[1] = ((setBoseReady !=0) & (boseSource!=0x11))<<1 && (boseOff==0);
  sendBuf[2] = boseMute?0xFF:boseVol;
  dispClient.write(sendBuf,sizeof(sendBuf));
  
  dispClientMillis = millis();
/*
  dispClient.print("Rdy:");
  dispClient.print(setBoseReady && boseSource!=0x11);
  
  dispClient.print(" Pwr:");
  dispClient.print(boseOff!=0);

  /*
  dispClient.print(" Mut:");
  dispClient.print(boseMute?"On ":"Off");
  */
/*
  dispClient.print(" Vol:");
  dispClient.print(boseMute?0xFF:boseVol);

/*
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
  */
}

void debugPrintHex(char c){
  if (c<0x10)
    debugClient.print("0");    
  debugClient.print(c, HEX);
  debugClient.print(" ");
}

void setBoseReady(const uint8_t newready){
  if (boseReady != newready){
    boseReady = newready;
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
        ctrlRoomState = 1;
      break;
    case 0x02: //Vol-
    case 0x03: //Vol+
      if (keyState){ // si relaché vérifier une dernière fois le volume
        ctrlVolume = 1;
        ctrlVolumePress = 0;
      }
      else{   //si enfoncé surveiller le volume
        ctrlVolume = 0;
        ctrlVolumePress = 1;
      }
      break;
    case 0x4C: //Power
      if (keyState !=0){ // si relaché vérifier room state
        if (boseOff)
          ctrlRoomState = 1;
        else   
          ctrlRoomStateScan = SCAN_INIT;
      }
      break;
  }
}

void decodResponse(){
  uint16_t opcode = buf[1]<<8 | buf[2];
  uint8_t sendToDisplayToDo = 0;
  // Cas d'erreur
  if (buf[0] & 0x80){
    if(debugClient.connected())
      debugClient.println("Bose return ERROR");
    setBoseReady(0);
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
        if (boseSource != buf[3]){
          boseSource = buf[3];
          sendToDisplayToDo = 1;
        }
        break;
      case 0x0111:      // Retour Room State
        if (boseOff != buf[5]){
          boseOff = buf[5];
          sendToDisplayToDo = 1;
          if (boseOff){ // Si devient actif alors controler la source
            if (ctrlRoomStateScan) //si devient inactif alors arrête surveillance arrêt
              ctrlRoomStateScan = 0;
          }
          else
            ctrlVolume = 1;
        }
        if (boseMute != buf[4]){
          boseMute = buf[4];
          sendToDisplayToDo = 1;
        }
        break;
      case 0x0115:      // Retour de Volume
        if (boseVol != buf[3]){
          boseVol = buf[3];
          sendToDisplayToDo = 1;
        }
        break;
      case 0x011b:    // Retour de etat KeyPresseEvent
        if (boseKeyEvent != buf[3]){
          boseKeyEvent = buf[3];
        }
        setBoseReady(1);
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
    for(uint8_t i=0; i< readlen; i++)
      debugPrintHex(buf[i]);
    debugClient.println();
  }
  
  if ((readlen+1) != len) {
    if(debugClient.connected())
      debugClient.println("Erreur de longueur");
    setBoseReady(0);
    return; //Pas reçu toute la longeur Exit
  }
  decodResponse();  

  // Reset control de vie
  waitState = WAITSTATE_INIT;
}

void sendToBose(const uint8_t* msg, uint8_t len){
  // interdiction de l'envoi si attente d'un ACK
  if (waitACK)
    return;

  if(debugClient.connected()) {
    debugClient.print("S: ");
    for(uint8_t i=0; i< len; i++)
      debugPrintHex(msg[i]);
    debugClient.println();
  }
    // now send to UART:
  Serial.write(msg, len);
  waitACK = WAITACK_INIT;
}
