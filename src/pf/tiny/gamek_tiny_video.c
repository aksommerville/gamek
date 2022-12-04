#include "gamek_tiny_internal.h"
#include <wiring_constants.h>
#include <variant.h>

typedef void GluedSercom;

GluedSercom *sercom_getPERIPH_SPI1();
GluedSercom *sercom_getSercom3();

void sercom_disableSPI(GluedSercom *s);
void sercom_enableSPI(GluedSercom *s);
void sercom_enableWIRE(GluedSercom *s);
void sercom_initMasterWIRE(GluedSercom *s,uint32_t a);
void sercom_initSPI(GluedSercom *s,int a,int b,int c,int d);
void sercom_initSPIClock(GluedSercom *s,int a,uint32_t b);
uint8_t sercom_isAddressMatch(GluedSercom *s);
uint8_t sercom_isDataReadyWIRE(GluedSercom *s);
uint8_t sercom_isMasterReadOperationWIRE(GluedSercom *s);
uint8_t sercom_isRestartDetectedWIRE(GluedSercom *s);
uint8_t sercom_isSlaveWIRE(GluedSercom *s);
uint8_t sercom_isStopDetectedWIRE(GluedSercom *s);
void sercom_prepareAckBitWIRE(GluedSercom *s);
void sercom_prepareCommandBitsWire(GluedSercom *s,uint8_t a);
uint8_t sercom_sendDataMasterWIRE(GluedSercom *s,uint8_t a);
uint8_t sercom_sendDataSlaveWIRE(GluedSercom *s,uint8_t a);
void sercom_setBaudrateSPI(GluedSercom *s,uint8_t a);
void sercom_setClockModeSPI(GluedSercom *s,uint8_t a);
uint8_t sercom_startTransmissionWIRE(GluedSercom *s,uint8_t a,uint8_t b);
uint8_t sercom_transferDataSPI(GluedSercom *s,uint8_t a);

static GluedSercom *spisc=0;

#define TSP_PIN_DC   22
#define TSP_PIN_CS   38
#define TSP_PIN_SHDN 27
#define TSP_PIN_RST  26
#define TSP_PIN_BT1  19
#define TSP_PIN_BT2  25
#define TSP_PIN_BT3  30
#define TSP_PIN_BT4  31

#define TS_SPI_SET_DATA_REG(x) {SERCOM4->SPI.DATA.bit.DATA=(x);}
#define TS_SPI_SEND_WAIT() {while(SERCOM4->SPI.INTFLAG.bit.DRE == 0);}

#define SPI_MIN_CLOCK_DIVIDER (uint8_t)(1 + ((F_CPU - 1) / 12000000))
#define SPI_MODE0 0x02
 
#define SPI_PAD_0_SCK_1 0
#define SPI_PAD_2_SCK_3 1
#define SPI_PAD_3_SCK_1 2
#define SPI_PAD_0_SCK_3 3

#define SERCOM_RX_PAD_0 0
#define SERCOM_RX_PAD_1 1
#define SERCOM_RX_PAD_2 2
#define SERCOM_RX_PAD_3 3

#define SERCOM_SPI_MODE_0 0
#define SERCOM_SPI_MODE_1 1
#define SERCOM_SPI_MODE_2 2
#define SERCOM_SPI_MODE_3 3

/* SPI.
 */
 
static void spi_begin() {
  spisc=sercom_getPERIPH_SPI1();
  
  // init,begin:
  pinPeripheral(PIN_SPI1_MISO, g_APinDescription[PIN_SPI1_MISO].ulPinType);
  pinPeripheral(PIN_SPI1_SCK, g_APinDescription[PIN_SPI1_SCK].ulPinType);
  pinPeripheral(PIN_SPI1_MOSI, g_APinDescription[PIN_SPI1_MOSI].ulPinType);
  
  // config:
  uint32_t clockFreq = (4000000 >= (F_CPU / SPI_MIN_CLOCK_DIVIDER) ? F_CPU / SPI_MIN_CLOCK_DIVIDER : 4000000);
  sercom_disableSPI(spisc);
  sercom_initSPI(spisc,PAD_SPI1_TX,PAD_SPI1_RX,0,0);
  sercom_initSPIClock(spisc,SERCOM_SPI_MODE_0,clockFreq);
  sercom_enableSPI(spisc);
}

static void spi_setClockDivider(uint8_t div) {
  if (div < SPI_MIN_CLOCK_DIVIDER) {
    sercom_setBaudrateSPI(spisc,SPI_MIN_CLOCK_DIVIDER);
  } else {
    sercom_setBaudrateSPI(spisc,div);
  }
}

static uint8_t spi_transfer(uint8_t v) {
  return sercom_transferDataSPI(spisc,v);
}

/* Private bits.
 */

static void startCommand(void) {
  digitalWrite(TSP_PIN_DC,LOW);
  digitalWrite(TSP_PIN_CS,LOW);
}

static void startData(void) {
  digitalWrite(TSP_PIN_DC,HIGH);
  digitalWrite(TSP_PIN_CS,LOW);
}

static void endTransfer(void) {
  digitalWrite(TSP_PIN_CS,HIGH);
}

static void setBrightness(uint8_t brightness) {
  if(brightness>15)brightness=15;  
  startCommand();
  spi_transfer(0x87);//set master current
  spi_transfer(brightness);
  endTransfer();
}

static void on(void) {
  digitalWrite(TSP_PIN_SHDN,HIGH);
  startCommand();//if _externalIO, this will turn boost converter on
  delay(10);
  spi_transfer(0xAF);//display on
  endTransfer();
}

static void off(void) {
  startCommand();
  spi_transfer(0xAE);//display off
  endTransfer();
  digitalWrite(TSP_PIN_SHDN,LOW);//SHDN
}

static void writeRemap(void) {
  uint8_t remap=(1<<5)|(1<<2);
  startCommand();
  spi_transfer(0xA0);//set remap
  spi_transfer(remap);
  endTransfer();
}

static void clearWindow(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {  
  if(x>95||y>63)return;
  uint8_t x2=x+w-1;
  uint8_t y2=y+h-1;
  if(x2>95)x2=95;
  if(y2>63)y2=63;
  
  startCommand();
  spi_transfer(0x25);//clear window
  spi_transfer(x);
  spi_transfer(y);
  spi_transfer(x2);
  spi_transfer(y2);
  endTransfer();
  delay(1);
}

/* Init.
 */
 
void tiny_video_init() {

  spi_begin();
  sercom_setClockModeSPI(spisc,SERCOM_SPI_MODE_0);
  spi_setClockDivider(4);
  
  pinMode(TSP_PIN_SHDN,OUTPUT);
  pinMode(TSP_PIN_DC,OUTPUT);
  pinMode(TSP_PIN_CS,OUTPUT);
  pinMode(TSP_PIN_RST,OUTPUT);
  digitalWrite(TSP_PIN_SHDN,LOW);
  digitalWrite(TSP_PIN_DC,HIGH);
  digitalWrite(TSP_PIN_CS,HIGH);
  digitalWrite(TSP_PIN_RST,HIGH);
  pinMode(TSP_PIN_BT1,INPUT_PULLUP);
  pinMode(TSP_PIN_BT2,INPUT_PULLUP);
  pinMode(TSP_PIN_BT3,INPUT_PULLUP);
  pinMode(TSP_PIN_BT4,INPUT_PULLUP);
  //reset
  digitalWrite(TSP_PIN_RST,LOW);
  delay(5);
  digitalWrite(TSP_PIN_RST,HIGH);
  delay(10);
  
  const uint8_t init[32]={0xAE, 0xA1, 0x00, 0xA2, 0x00, 0xA4, 0xA8, 0x3F,
  0xAD, 0x8E, 0xB0, 0x0B, 0xB1, 0x31, 0xB3, 0xF0, 0x8A, 0x64, 0x8B,
  0x78, 0x8C, 0x64, 0xBB, 0x3A, 0xBE, 0x3E, 0x81, 0x91, 0x82, 0x50, 0x83, 0x7D};
  off();
  startCommand();
  for(uint8_t i=0;i<32;i++) spi_transfer(init[i]);
  endTransfer();
  //use libarary functions for remaining init
  setBrightness(10);
  writeRemap();
  clearWindow(0,0,96,64);
  on();
  
}

/* Send framebuffer.
 */
 
void tiny_video_swap(const uint8_t *fb) {
  startData();
  int16_t i=GAMEK_TINY_FBW*GAMEK_TINY_FBH;
  for (;i-->0;fb++) {
    TS_SPI_SET_DATA_REG(*fb);
    TS_SPI_SEND_WAIT();
  }
  endTransfer();
}
