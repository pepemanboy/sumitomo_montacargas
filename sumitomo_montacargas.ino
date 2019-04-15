/**
 * Montacargas
 * 
 * Master: Puerta
 * Slave: Montacargas
 */

#include "ciropkt.h"
#include "ciropkt_cmd.h"
#include <SoftwareSerial.h>

/// CONFIGURABLE DEFINES
#define MASTER
// #define DEBUG
#define MCG_DOOR_POLARITY 0

/// COMMUNICATION
#define ADDRESS 0

/// SERIAL
#define SERIAL_TIMEOUT_MS 100

/// HC12
#define HC12_CHANNEL 1
#define HC12_BAUDRATE 9600
#define HC12 HC12_Serial

#define HC12_RX 2
#define HC12_TX 3
#define HC12_SET 7

/// USB
#define USB Serial
#define USB_BAUDRATE 9600

/// LED
#define LED_GREEN 10
#define LED_RED 11
#define LED_ERROR LED_RED

/// IO
#define IO_PIN_1 12
#define IO_PIN_2 13

#define DOOR_1 IO_PIN_1
#define DOOR_2 IO_PIN_2
#define RELAY IO_PIN_1

/// COMMUNICATIONS
/// RX
char rxBuf_[50]; // Min size pkt_MAXSPACE + 1
pkt_VAR(rxPacket_);
MCGControl * rxMCGControl_ = (MCGControl *) &rxPacket_.data;
size_t rxCnt_ = 0;
unsigned long serialTimestamp_ = 0;

/// TX
#ifdef MASTER
char txBuf_[pkt_MAXSPACE + 1];
size_t txBufLen_ = 0;
pkt_VAR(txPacket_);
#endif // MASTER

/// SOFTWARE SERIAL
SoftwareSerial HC12_Serial(HC12_RX, HC12_TX);

//// SHARED FUNCTIONS

/// DEBUG AND ERROR FUNCTIONS
/** Debug to PC Serial */
void debug(char * str)
{
  #ifdef DEBUG
  USB.println(str);
  #endif // DEBUG
}

/** Debug error */
void debugError(char * str, res_t error_code)
{
  #ifdef DEBUG
  USB.print(str);
  USB.print(F(" Error code = "));
  USB.println(error_code);
  #endif // DEBUG
}

/** Error function */
void error()
{
  while(1)
  {
    digitalWrite(LED_ERROR, HIGH);
    delay(500);
    digitalWrite(LED_ERROR, LOW);
    delay(500);
  }
}

/// HC12 MODULE
/** Configure HC 12 parameter */
res_t HC12_configure_parameter(char * query)
{
  res_t r;
  HC12.println(query);
  size_t l = HC12.readBytes(rxBuf_, sizeof(rxBuf_));
  rxBuf_[l] = '\0';
  debug(rxBuf_);
  if (!strstr(rxBuf_, "OK")) 
    return EFormat;
  return Ok;
}

/** Setup HC12 module */
res_t HC12_setup()
{
  res_t r;
  pinMode(HC12_SET, OUTPUT);

  HC12.setTimeout(SERIAL_TIMEOUT_MS);
  HC12.begin(9600); // Default baudrate per datasheet
  
  debug((char *)"Setting set pin LOW");
  digitalWrite(HC12_SET, LOW); // Enter setup mode
  delay(250);

  char buf[20];
  // Test
  debug((char *)"printing AT");
  sprintf(buf, "AT");
  r = HC12_configure_parameter(buf);
  if (r != Ok) {
    debugError((char *)"Error testing HC12", r);
    return r;
  }

  // Set channel
  debug((char *)"Configuring channel");
  sprintf(buf, "AT+C%03d", HC12_CHANNEL); 
  r = HC12_configure_parameter(buf);
  if (r != Ok) {
    debugError((char *)"Error setting channel HC12", r);
    return r;
  }
    
  // Set baud rate
  debug((char *)"Configuring baudrate");
  sprintf(buf, "AT+B%d", HC12_BAUDRATE);
  r = HC12_configure_parameter(buf);
  if (r != Ok) {
    debugError((char *)"Error setting baudrate HC12", r);
    return r;
  }
    
  HC12.begin(HC12_BAUDRATE);  

  debug((char *)"Setting pin HIGH");
  digitalWrite(HC12_SET, HIGH); // Exit setup mode
  delay(250);
  
  return Ok;
}

/// LED MODULE
typedef enum
{
  led_On = 0,
  led_Off,
}led_e;

/** Setup led module */
res_t ledSetup()
{
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  ledControl(LED_GREEN, led_Off);
  ledControl(LED_RED, led_Off);
  ledGoodBad(false);
  return Ok;
}

/** Contro leds */
void ledControl(uint8_t led, uint8_t state)
{
  digitalWrite(led, state == led_On ? LOW : HIGH);
}

/** Show good or bad */
void ledGoodBad(bool good)
{
  ledControl(good ? LED_GREEN : LED_RED, led_On);
  ledControl(good ? LED_RED : LED_GREEN, led_Off);
}

/// RELAY MODULE
/** Control relay */
void relayControl(mcg_e state)
{
  digitalWrite(RELAY, state == mcg_On ? LOW : HIGH);
}

/// IO MODULE
/** Setup IOS */
res_t IOSetup()
{
  // Puerta
  #ifdef MASTER
  pinMode(DOOR_1, INPUT);
  pinMode(DOOR_2, INPUT);
  #endif // MASTER

  // Montacargas
  #ifndef MASTER
  pinMode(RELAY, OUTPUT);
  #endif // !MASTER

  return Ok;
}

/// MCG MODULE
typedef enum
{
  mcg_door_closed = 1,
  mcg_door_open,
}mcg_door_e;

/** Read door status */
uint8_t mcgReadDoor(uint8_t door)
{
  return digitalRead(door) == MCG_DOOR_POLARITY ? mcg_door_closed : mcg_door_open;
}

/** Check door status */
mcg_e mcgDoorCommand()
{
  mcg_e c = (((mcgReadDoor(DOOR_1) == mcg_door_closed) && (mcgReadDoor(DOOR_2) == mcg_door_closed)) ? mcg_Off : mcg_On);
  ledGoodBad(c == mcg_Off);
  return c;
}

/** Control mcg */
res_t mcgControl(mcg_e cmd)
{
  relayControl(cmd);
  ledGoodBad(cmd == mcg_Off);
  return Ok;
}

/// RX
#ifndef MASTER
/** Process received packet */
res_t packetRx()
{
  packet_t * p = &rxPacket_;
  res_t r = pktDeserialize(p, (uint8_t *)rxBuf_, rxCnt_);
  if (!r) {
    debugError((char *)"Error parsing packet", r);
    return EParse; 
  }
  else
  {       
    if (pktCheck(p))
    {
      if (p->address != ADDRESS) {
        debug((char *)"Mismatch address");
        return EAddress;
      }
      if (p->command != cmd_MCGControl) {
        debug((char *)"Mismatch command");
        return ECommand;
      }
      mcgControl((mcg_e)rxMCGControl_->cmd);
    }
    else {
      debug((char *)"Mismatch CRC");
      return EFormat;
    }
  }   
}

/** Read loop */
void readLoop()
{    
  if (HC12.available()) 
  {
    serialTimestamp_ = millis();
    char inByte = HC12.read();
    if(inByte == 0)
    {
      packetRx();     
      rxCnt_ = 0;         
    }
    else
      rxBuf_[rxCnt_++] = inByte;
  }

  // Serial timeout
  if ((rxCnt_ != 0) && ((millis() - serialTimestamp_) > SERIAL_TIMEOUT_MS))
    rxCnt_ = 0;

  // Serial overflow
  if (rxCnt_ >= sizeof(rxBuf_))
    rxCnt_ = 0;
}
#endif // !MASTER

/// TX
#ifdef MASTER
/** Setup MCG packet */
void setupMCGpacket(packet_t *p, mcg_e mcg_cmd)
{
  p->address = ADDRESS;
  p->command = cmd_MCGControl;
  
  MCGControl c = {.cmd = mcg_cmd};
  pktUpdate(p, &c, sizeof(MCGControl));

  pktRefresh(p);
}

/** Move packet to buffer */
void bufferPacket(packet_t *p)
{
  size_t len = sizeof(txBuf_);
  pktSerialize(p, (uint8_t *) txBuf_, &len);
  txBufLen_ = len;
}

/** Send buffered packet */
void sendPacket(packet_t *p)
{
  bufferPacket(p);
  sendBuffer();
}

/** Send txbuf */
void sendBuffer()
{
  HC12.write(txBuf_, txBufLen_);
  HC12.write((uint8_t)0);  
}
#endif // MASTER

//// SETUP
void setup() 
{
  #ifdef DEBUG  
  USB.begin(USB_BAUDRATE);
  #endif // DEBUG
  
  uint8_t res = Ok;
  res = ledSetup();
  if (res != Ok)
    error();

  res = HC12_setup();
  if (res != Ok)
    error();

  res = IOSetup();
  if (res != Ok)
    error();
}

//// LOOP
void loop()
{  
  // puerta
  #ifdef MASTER
  setupMCGpacket(&txPacket_, mcgDoorCommand());
  bufferPacket(&txPacket_);
  sendBuffer();
  delay(200);
  #endif

  // montacargas
  #ifndef MASTER
  readLoop();
  #endif
}
