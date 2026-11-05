#ifndef displaySSD1327_h
#define displaySSD1327_h

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>

#define DISPLAY_MODEL_NAME "SSD1327"

typedef GFXcanvas1 Canvas;
typedef Adafruit_SSD1327 yoDisplay;

      /*
      SSD1327_GRAYTABLE,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
      0x07, 0x08, 0x10, 0x18, 0x20, 0x2f, 0x38, 0x3f,
      */

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
