#ifndef bose_h
#define bose_h
#include <WiFiClient.h>
#include <WiFiServer.h>

#define DEBUG_PORT  9977
#define DISP_PORT   9978

const uint8_t cmdEnableKeyEvent[]   = {0x07, 0x00, 0x01, 0x1b, 0x01, 0x0d, 0x11};
const uint8_t cmdGetKeyEvent[]      = {0x07, 0x00, 0x01, 0x1b, 0x00, 0x00, 0x1d};
const uint8_t cmdGetVolume[]        = {0x07, 0x00, 0x01, 0x15, 0x00, 0x1e, 0x0d};
const uint8_t cmdGetRmState[]       = {0x08, 0x00, 0x01, 0x11, 0x00, 0x02, 0x00, 0x1A};
const uint8_t cmdGetSource[]        = {0x08, 0x00, 0x01, 0x0A, 0x00, 0x18, 0x18, 0x03};

// 0x3E8 1000*10ms = 10s
#define SEQ_COUNT 0xBB8 
// 250x10ms = 2.5 s
#define ACK_COUNT 0xFA
#define CTRLVOL_COUNT 6

// 20 caractètes sont suffisant par rapport au 255 théorique
#define MAXPACKETSIZE 20

class Bose{
  public:
    Bose();
    void begin();
    void doloop();
    
  private:
    WiFiServer* dispServer = new WiFiServer(DISP_PORT); //DISP_PORT);
    WiFiClient dispClient;
    
    WiFiServer* debugServer = new WiFiServer(DEBUG_PORT); //DEBUG_PORT);
    WiFiClient debugClient;
  
    //Bose State
    uint8_t boseOnError = 0;
    uint8_t boseKeyEvent = 0;

    uint8_t boseOff=0xFF;     //Par défaut considéré comme OFF
    void    setOff(const uint8_t value);
    uint8_t boseVol = 0;
    void    setVolume(const uint8_t value);
    uint8_t boseMute = 0;
    void    setMute(const uint8_t value);
    uint8_t boseSource = 0;
    void    setSource(const uint8_t value);
    uint8_t timeOut = 1;  //Par défaut en TimeOut
    void    setTimeOut(const uint8_t value);

  private:
    void notifyPower();
    void notifyVolume();
    void notifySource();
    
  private:
    uint8_t waitACK = 0;
    uint8_t buf[MAXPACKETSIZE];

    uint16_t seqCnt = 0;
    uint8_t seqIndex = 0;

    uint8_t ctrlVolume = 0;
    uint8_t ctrlVolumeAct = 0;
    uint8_t ctrlRmState = 0;
    uint8_t ctrlSource = 0;

    void sendBose(const uint8_t* msg, uint8_t len);
    void receiveBose();
    void decodResponse();
    void decodKey(uint8_t keyCode, uint8_t keyState);
    
    Stream* streamRef;
    void debugPrintHex(char c);
};
#endif
