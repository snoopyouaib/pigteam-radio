#ifndef displayILI9341_h
#define displayILI9341_h

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// ILI9341 — hardware-fixed resolution.
#undef DSP_WIDTH
#undef DSP_HEIGHT
#define DSP_WIDTH  320
#define DSP_HEIGHT 240
#define DISPLAY_MODEL_NAME "ILI9341"

typedef GFXcanvas16 Canvas;
typedef Adafruit_ILI9341 yoDisplay;

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
