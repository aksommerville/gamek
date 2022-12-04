#include "gamek_tiny_internal.h"
#include <variant.h>

/* Read hardware registers.
 */

static int tcIsSyncing() {
  return TC5->COUNT16.STATUS.reg&TC_STATUS_SYNCBUSY;
}

static void tcReset() {
  TC5->COUNT16.CTRLA.reg=TC_CTRLA_SWRST;
  while (tcIsSyncing());
  while (TC5->COUNT16.CTRLA.bit.SWRST);
}

/* Init.
 */
 
void tiny_audio_init() {

  mynth_init(GAMEK_TINY_AUDIO_RATE);

  analogWrite(A0,0);
  
  // Enable GCLK for TCC2 and TC5 (timer counter input clock)
  GCLK->CLKCTRL.reg=(GCLK_CLKCTRL_CLKEN|GCLK_CLKCTRL_GEN_GCLK0|GCLK_CLKCTRL_ID(GCM_TC4_TC5));
  while (GCLK->STATUS.bit.SYNCBUSY);
  tcReset();
  
  // Set Timer counter Mode to 16 bits
  TC5->COUNT16.CTRLA.reg|=TC_CTRLA_MODE_COUNT16;
  
  // Set TC5 mode as match frequency
  TC5->COUNT16.CTRLA.reg|=TC_CTRLA_WAVEGEN_MFRQ;
  TC5->COUNT16.CTRLA.reg|=TC_CTRLA_PRESCALER_DIV1;
  TC5->COUNT16.CC[0].reg=(SystemCoreClock/GAMEK_TINY_AUDIO_RATE-1);
  while (tcIsSyncing());
  
  // Configure interrupt request
  NVIC_DisableIRQ(TC5_IRQn);
  NVIC_ClearPendingIRQ(TC5_IRQn);
  NVIC_SetPriority(TC5_IRQn,0);
  NVIC_EnableIRQ(TC5_IRQn);
  
  // Enable the TC5 interrupt request
  TC5->COUNT16.INTENSET.bit.MC0=1;
  while (tcIsSyncing());
  
  // Enable TC
  TC5->COUNT16.CTRLA.reg|=TC_CTRLA_ENABLE; // Fails here if TC5_Handler unset or unpopulated.
  while (tcIsSyncing());
}

/* Interrupt handlers.
 */
 
static volatile uint8_t tiny_audio_mutex=0;
static int16_t tiny_previous_sample=0;
 
void TC5_Handler() {
  while (DAC->STATUS.bit.SYNCBUSY==1);
  
  int16_t sample=0;
  if (tiny_audio_mutex) sample=tiny_previous_sample;
  else mynth_update(&sample,1);
  
  DAC->DATA.reg=((sample>>6)+0x200)&0x3ff;
  while (DAC->STATUS.bit.SYNCBUSY==1);
  TC5->COUNT16.INTFLAG.bit.MC0=1;
}

/* Receive event from client.
 */

void gamek_platform_audio_event(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b) {
  tiny_audio_mutex=1;
  mynth_event(chid,opcode,a,b);
  tiny_audio_mutex=0;
}

void gamek_platform_play_song(const void *v,uint16_t c) {
  tiny_audio_mutex=1;
  mynth_play_song(v,c);
  tiny_audio_mutex=0;
}

void gamek_platform_set_audio_wave(uint8_t waveid,const int16_t *v,uint16_t c) {
  if (c==512) {
    tiny_audio_mutex=1;
    mynth_set_wave(waveid,v);
    tiny_audio_mutex=0;
  }
}

void gamek_platform_audio_configure(const void *v,uint16_t c) {
  // Mynth doesn't have config like this.
}
