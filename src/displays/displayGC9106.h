#ifndef displayGC9106_h
#define displayGC9106_h
#include "../core/options.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "../libraries/Adafruit_GC9106Ex/Adafruit_GC9106Ex.h"       // https://github.com/prenticedavid/Adafruit_GC9102_kbv

// GC9106 — hardware-fixed resolution.
#undef DSP_WIDTH
#undef DSP_HEIGHT
#define DSP_WIDTH  160
#define DSP_HEIGHT 80
#define DISPLAY_MODEL_NAME "GC9106"

typedef GFXcanvas16 Canvas;
typedef Adafruit_GC9106Ex yoDisplay;

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
