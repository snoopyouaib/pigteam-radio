#ifndef displaySH1106_h
#define displaySH1106_h

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

typedef GFXcanvas1 Canvas;

#if DSP_MODEL==DSP_SH1106
  #define DISPLAY_MODEL_NAME "SH1106"
  typedef Adafruit_SH1106G yoDisplay;
#else
  #define DISPLAY_MODEL_NAME "SH1107"
  typedef Adafruit_SH1107 yoDisplay;
#endif

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
