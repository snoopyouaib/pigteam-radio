#ifndef dspcolors_h
#define dspcolors_h
#pragma once

// Centralized display color/palette macros selected by active DSP model.
#if DSP_MODEL==DSP_SH1106 || DSP_MODEL==DSP_SH1107
  #define BOOT_PRG_COLOR    SH110X_WHITE
  #define BOOT_TXT_COLOR    SH110X_WHITE
  #define SILVER            SH110X_WHITE
  #define TFT_BG            SH110X_BLACK
  #define TFT_FG            SH110X_WHITE
  #define TFT_LOGO          SH110X_WHITE

#elif DSP_MODEL==DSP_SSD1306 || DSP_MODEL==DSP_SSD1305 || DSP_MODEL==DSP_SSD1322 || DSP_MODEL==DSP_ST7920
  #define BOOT_PRG_COLOR    WHITE
  #define BOOT_TXT_COLOR    WHITE
  #define SILVER            WHITE
  #define TFT_BG            BLACK
  #define TFT_FG            WHITE
  #define TFT_LOGO          WHITE

#elif DSP_MODEL==DSP_NOKIA5110
  #define BOOT_PRG_COLOR    BLACK
  #define BOOT_TXT_COLOR    BLACK
  #define SILVER            BLACK
  #define TFT_BG            WHITE
  #define TFT_FG            BLACK
  #define TFT_LOGO          BLACK

#elif DSP_MODEL==DSP_SSD1327
  #define BOOT_PRG_COLOR    0x07
  #define BOOT_TXT_COLOR    0x3f
  #define DARK_GRAY         0x01
  #define SILVER            0x07
  #define TFT_BG            0x00
  #define TFT_FG            0x08
  #define TFT_LOGO          0x3f
  #define ORANGE            0x05

#elif DSP_MODEL==DSP_1602 || DSP_MODEL==DSP_2004
  #define BOOT_PRG_COLOR    0x1
  #define BOOT_TXT_COLOR    0x1

#else
  #define BOOT_PRG_COLOR    0xE68B
  #define BOOT_TXT_COLOR    0xFFFF
#endif

#ifndef TFT_BG
  #define TFT_BG            BLACK
#endif

#ifndef TFT_FG
  #define TFT_FG            WHITE
#endif

#ifndef TFT_LOGO
  #define TFT_LOGO          TFT_FG
#endif

#endif