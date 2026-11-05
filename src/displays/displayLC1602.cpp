#include "../core/options.h"
#if DSP_MODEL==DSP_1602 || DSP_MODEL==DSP_2004
#include "dspcore.h"
#include <WiFi.h>
#include "../core/config.h"
#include "../core/utility.h"

#ifndef SCREEN_ADDRESS
  #define SCREEN_ADDRESS 0x27 ///< See datasheet for Address or scan it https://create.arduino.cc/projecthub/abdularbi17/how-to-scan-i2c-address-in-arduino-eaadda
#endif

DspCore::DspCore(): DSP_INIT {}

void DspCore::apScreen() {
  clear();
  setCursor(0,0);
  print(const_lcdApMode);
  setCursor(0,1);
  print(utility.ipToStr(WiFi.softAPIP()));
#if DSP_WIDTH==20 && DSP_HEIGHT==4
  setCursor(0, 2);
  print(const_lcdApName);
  print(apSsid);
  setCursor(0, 3);
  print(const_lcdApPass);
  print(apPassword);
#endif
}

void DspCore::initDisplay() {
#ifdef LCD_I2C
  init();
  backlight();
#else
  #if DSP_WIDTH==20 && DSP_HEIGHT==4
    begin(20, 4);
  #else
    begin(16, 2);
  #endif
#endif
  clearClipping();
}

void DspCore::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
  if(w<2) return;
  char buf[width()+1] = { 0 };
  snprintf(buf, sizeof(buf), "%*s%s", w-1, "", " ");
  setCursor(x, y);
  print(buf);
  setCursor(x, y);
}

uint16_t DspCore::width(){
#if DSP_WIDTH==20 && DSP_HEIGHT==4
  return 20;
#else
  return 16;
#endif
}

uint16_t DspCore::height(){
#if DSP_WIDTH==20 && DSP_HEIGHT==4
  return 4;
#else
  return 2;
#endif
}

void DspCore::clearDsp(bool black){ clear(); }
void DspCore::flip(){ }
void DspCore::invert(){ }
void DspCore::sleep(void) { 
  noDisplay();
#ifdef LCD_I2C
  noBacklight();
#endif
}
void DspCore::wake(void) { 
  display();
#ifdef LCD_I2C
  backlight();
#endif
}

#endif
