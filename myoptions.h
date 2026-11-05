#ifndef myoptions_h
#define myoptions_h

/* ------------------------------------------------------------------
   ehRadio - ESP32-S3 N16R8 + PCM5102 (I2S)
   Rappel N16R8 : GPIO 35, 36, 37 interdits (PSRAM octal), 26-32 reserves.

   CHOIX DE L'ECRAN (un seul a la fois) :
     1 = petit OLED 1,3" I2C (SH1106, 128x64), sans tactile
     0 = TFT ILI9341 240x320 SPI + tactile XPT2046
   ------------------------------------------------------------------ */
#define ECRAN_OLED 0

#if ECRAN_OLED
  /* OLED 1,3" I2C : SDA -> GPIO 4, SCL -> GPIO 5, VCC 3,3 V, GND.
     Si l'image est decalee ou brouillee, essaie DSP_SSD1306 a la place.
     Adresse I2C 0x3C par defaut ; si l'ecran reste noir, certains modules
     sont en 0x3D (voir SCREEN_ADDRESS dans options.h). */
  #define DSP_MODEL     DSP_SH1106
  #define I2C_SDA       4
  #define I2C_SCL       5
#else
  /* TFT ILI9341 + tactile XPT2046 (bus SPI A : SCK=12, MISO=13, MOSI=11) */
  #define DSP_MODEL     DSP_ILI9341
  #define SPIA_DEFAULT
  #define TFT_CS        10
  #define TFT_DC         9
  #define TFT_RST        8
  #define TS_MODEL      TS_MODEL_XPT2046
  #define TS_SPI        'A'
  #define TS_CS         14
#endif

/* Decodeur : DAC I2S PCM5102 (VS1053 desactive) */
#define I2S_DOUT      16   /* PCM5102 DIN */
#define I2S_BCLK      17   /* PCM5102 BCK */
#define I2S_LRC       15   /* PCM5102 LCK (LRCK / WS) */
#define VS1053_CS     255  /* 255 = VS1053 non utilise */

/* Recherche Radio-Browser : tampon de telechargement 32 Ko (PSRAM). */
#define SEARCHRESULTS_BUFFER 32

/* Police de l'horloge : YO_MONO = 7 segments fins façon yoRadio.
   Autres choix : CHUNKY6_PX (gros pixels, defaut) ou CHUNKY6.
   CLOCKGLOW = fond "afficheur allume" derriere les chiffres. */
#define CLOCKFONT YO_MONO
#define CLOCKGLOW true

#endif
