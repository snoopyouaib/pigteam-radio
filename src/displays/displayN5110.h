#ifndef displayN5110_h
#define displayN5110_h

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Nokia 5110 — hardware-fixed resolution.
#undef DSP_WIDTH
#undef DSP_HEIGHT
#define DSP_WIDTH  84
#define DSP_HEIGHT 48
#define DISPLAY_MODEL_NAME "N5110"

typedef GFXcanvas1 Canvas;
typedef Adafruit_PCD8544 yoDisplay;

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
