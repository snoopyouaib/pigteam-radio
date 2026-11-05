#ifndef displayGC9A01A_h
#define displayGC9A01A_h
#include "../core/options.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// GC9A01A — hardware-fixed round display.
#undef DSP_WIDTH
#undef DSP_HEIGHT
#define DSP_WIDTH  240
#define DSP_HEIGHT 240
#define DISPLAY_MODEL_NAME "GC9A01A"

typedef GFXcanvas16 Canvas;
typedef Adafruit_GC9A01A yoDisplay;

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
