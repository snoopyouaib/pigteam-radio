#ifndef displayLC1602_h
#define displayLC1602_h

#include <Arduino.h>

#define DSP_NOT_FLIPPED

// LCD character displays — dimensions are hardware-fixed.
#undef DSP_WIDTH
#undef DSP_HEIGHT
#if DSP_MODEL==DSP_2004
  #define DSP_WIDTH  20
  #define DSP_HEIGHT 4
  #define DISPLAY_MODEL_NAME "2004"
#else
  #define DSP_WIDTH  16
  #define DSP_HEIGHT 2
  #define DISPLAY_MODEL_NAME "1602"
#endif

// Auto-detect interface from pins: I2C if both I2C pins defined, else parallel
#if I2C_SDA!=255 && I2C_SCL!=255
  #define LCD_I2C
  #include "../libraries/LiquidCrystalI2C/LiquidCrystalI2CEx.h"
#else
  #include <LiquidCrystal.h>
#endif

#ifdef LCD_I2C
  typedef LiquidCrystal_I2C yoDisplay;
  #if DSP_WIDTH==20 && DSP_HEIGHT==4
    #define DSP_INIT LiquidCrystal_I2C(SCREEN_ADDRESS, 20, 4, I2C_SDA, I2C_SCL)
  #else
    #define DSP_INIT LiquidCrystal_I2C(SCREEN_ADDRESS, 16, 2, I2C_SDA, I2C_SCL)
  #endif
#else
  typedef LiquidCrystal yoDisplay;
  #define DSP_INIT LiquidCrystal(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7)
#endif

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
