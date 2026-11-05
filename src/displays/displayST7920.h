#ifndef displayST7920_h
#define displayST7920_h

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "../libraries/ST7920/ST7920.h"

// ST7920 — hardware-fixed resolution.
#undef DSP_WIDTH
#undef DSP_HEIGHT
#define DSP_WIDTH  128
#define DSP_HEIGHT 64
#define DISPLAY_MODEL_NAME "ST7920"

typedef GFXcanvas1 Canvas;
typedef ST7920 yoDisplay;

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
