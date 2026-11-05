#ifndef displaySSD1322_h
#define displaySSD1322_h

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "../libraries/SSD1322/SSD1322.h"

#define DISPLAY_MODEL_NAME "SSD1322"

typedef GFXcanvas1 Canvas;
typedef Jamis_SSD1322 yoDisplay;

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
