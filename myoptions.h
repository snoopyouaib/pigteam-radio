#ifndef myoptions_h
#define myoptions_h

/* ------------------------------------------------------------------
   ehRadio - PigTeam - 3 cartes, choisies par l'environnement PlatformIO :
     ehradio_s3_n16r8      : ESP32-S3 N16R8 + ILI9341 tactile (ou OLED) + PCM5102
     ehradio_s3_zero       : ESP32-S3-Zero 4 Mo + OLED 1,3" + rotatif + 2 boutons
     ehradio_s3_tiny_tft   : ESP32-S3-Tiny N8R8 + TFT ST7789 + rotatif + BACK

   Commandes des cartes SANS tactile (Zero et Tiny) :
     rotatif : tourner = volume
               clic    = ouvrir la liste des stations (patch controls.cpp, voir ENC_CLICK_OPENS_LIST)
               dans la liste : tourner = choisir, clic = valider et sortir
     bouton lecture : clic = lecture / arret (en liste : valide et sort), appui long = liste <-> lecteur
   ------------------------------------------------------------------ */

#ifdef CARTE_ZERO
/* ------------------------------------------------------------------
   Env ehradio_s3_zero : ESP32-S3-Zero (4 Mo flash, 2 Mo PSRAM) soudee sur le
   module OLED 1,3" + encodeur EC11 + 2 boutons (celui du chrono piste).
   Cablage du module (meme brochage que le chrono piste) :
     CONFIRM -> GPIO 7    OLED SDA -> GPIO 8    OLED SCL -> GPIO 9
     PUSH    -> GPIO 10   CLK (A)  -> GPIO 11   DT (B)    -> GPIO 12
     BACK    -> GPIO 13
   PCM5102 : DIN -> GPIO 4, BCK -> GPIO 5, LCK -> GPIO 6
   (GPIO 4/5/6 etaient la carte SD du chrono : libres ici)
   Evites : 0 et 3 (strapping), 19/20 (USB natif).
   ------------------------------------------------------------------ */
  #define DSP_MODEL     DSP_SH1106    /* si image decalee/brouillee : DSP_SSD1306 */
  #define I2C_SDA       8
  #define I2C_SCL       9

  /* Rotatif = encodeur 1 : tourner = volume, clic = liste des stations */
  #define ENC_CLK       11
  #define ENC_DT        12
  #define ENC_SW        10
  #define ENC_STEPS     4             /* EC11 : 4 transitions par cran (2 = saute de 2 stations par cran) */
  #define ENC_CLICK_OPENS_LIST        /* clic rotatif = liste (necessite le patch controls.cpp) */
  #define BTN_PLAY      7             /* CONFIRM : lecture / arret */
  #define BTN_MODE      13            /* BACK : double-clic = muet, appui long = veille */

  #define I2S_DOUT      4             /* PCM5102 DIN */
  #define I2S_BCLK      5             /* PCM5102 BCK */
  #define I2S_LRC       6             /* PCM5102 LCK */
  #define VS1053_CS     255
#elif defined(CARTE_TINY_TFT)
/* ------------------------------------------------------------------
   Env ehradio_s3_tiny_tft : ESP32-S3-Tiny N8R8 (8 Mo flash, 8 Mo PSRAM octale)
   soude sur le module TFT ST7789 320x240 (montage final du chrono piste TFT).
   On reprend le cablage du chrono ; le GPS (5/6) ne sert pas ici, la SD (7-10) oui.
     TFT       : SCLK 11, MOSI 12, RST 13, DC 14, CS 15, BL 16 (pas de MISO)
     Encodeur  : CLK 17, DT 18, PUSH 44 (RX)      BACK : 43 (TX)
     Batterie  : GPIO 1 (pont 10k/10k)
     Carte SD  : CS 10, MOSI 7, MISO 8, SCK 9 (bus SPI B, separe de l'ecran)
     PCM5102   : DIN -> 4, BCK -> 5, LCK -> 6  (GPIO 2 et 3 restent libres)
     Debrancher le GPS des GPIO 5/6 avant de brancher le DAC.
   PSRAM octale : GPIO 33-37 interdits (non exposes sur cette carte).
   ------------------------------------------------------------------ */
  #define DSP_MODEL     DSP_ST7789    /* si couleurs inversees : #define DSP_INVERT_QUIRK true */
  #define SPIA_SCK      11
  #define SPIA_MOSI     12
  #define SPIA_MISO     255           /* pas de MISO sur l'ecran */
  #define TFT_CS        15
  #define TFT_DC        14
  #define TFT_RST       13

  /* Retroeclairage reglable (PWM sur BL) + mesure batterie (diviseur 10k/10k = x2) */
  #define BRIGHTNESS_PIN        16
  #define DSP_DIMMING_ENABLED   true
  #define BATTERY_PIN           1
  #define BATTERY_DIVIDER_RATIO 2.0

  /* Rotatif = encodeur 1 : tourner = volume, clic = liste des stations
       dans la liste : tourner = choisir, clic = valider et sortir
     BACK = BTN_PLAY : clic = lecture / arret, appui long = liste <-> lecteur */
  #define ENC_CLK       17
  #define ENC_DT        18
  #define ENC_SW        44
  #define ENC_STEPS     4             /* EC11 : valeurs permises 1, 2 ou 4 seulement */
  #define ENC_CLICK_OPENS_LIST        /* clic rotatif = liste (necessite le patch controls.cpp) */
  #define BTN_PLAY      43

  #define I2S_DOUT      4             /* PCM5102 DIN */
  #define I2S_BCLK      5             /* PCM5102 BCK */
  #define I2S_LRC       6             /* PCM5102 LCK */
  #define VS1053_CS     255

  /* Carte SD (FAT32) sur son propre bus SPI B : lecture de MP3 hors Wi-Fi */
  #define SPIB_SCK      9
  #define SPIB_MISO     8
  #define SPIB_MOSI     7
  #define SD_SPI        'B'
  #define SD_CS         10
#else
/* ------------------------------------------------------------------
   Env ehradio_s3_n16r8 : ESP32-S3 N16R8 + PCM5102 (I2S)
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
#endif

/* Recherche Radio-Browser : tampon de telechargement 32 Ko (PSRAM). */
#define SEARCHRESULTS_BUFFER 32

/* Police de l'horloge : YO_MONO = 7 segments fins facon yoRadio.
   Autres choix : CHUNKY6_PX (gros pixels, defaut) ou CHUNKY6.
   CLOCKGLOW = fond "afficheur allume" derriere les chiffres. */
#define PIGTEAM_LOGO
#define CLOCKFONT YO_MONO
#define CLOCKGLOW true
#define SCROLLTIME 30   /* ms par pas : 20 = defaut (100 px/s), 30 = ~67 px/s, 40 = 50 px/s */

#endif
