#include "options.h"
#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
#include <Arduino.h>
#include "config.h"
#include "controls.h"
#include "slideshow.h"
#include "touchmenu.h"
#include "display.h"
#include "logging.h"
#include "network.h"
#include "utility.h"
#include "player.h"
#include "backlightcontrols.h"
#include "touchscreen.h"

#ifndef TS_X_MIN
  #define TS_X_MIN              400
#endif
#ifndef TS_X_MAX
  #define TS_X_MAX              3800
#endif
#ifndef TS_Y_MIN
  #define TS_Y_MIN              260
#endif
#ifndef TS_Y_MAX
  #define TS_Y_MAX              3800
#endif
#ifndef TS_STEPS
  #define TS_STEPS              40
#endif
#ifndef TS_SWIPE_THRESHOLD_PX
  #if TS_MODEL == TS_MODEL_XPT2046
    #define TS_SWIPE_THRESHOLD_PX  20   // resistive: needs jitter filtering
  #else
    #define TS_SWIPE_THRESHOLD_PX  12   // capacitive (GT911, FT6336): clean signal
  #endif
#endif
#ifndef TS_COOLDOWN_MS
  #define TS_COOLDOWN_MS         150
#endif
#ifndef TS_DEEPSLEEP_MS
  #define TS_DEEPSLEEP_MS        5000
#endif
#ifndef TS_DOUBLETAP_MS
  #define TS_DOUBLETAP_MS        400
#endif
#ifndef TS_BRIGHTNESS_MIN
  #define TS_BRIGHTNESS_MIN      5     // PigTeam : luminosite minimale par glissement (evite l'ecran noir)
#endif
#ifndef TS_MENU_ZONE_W
  #define TS_MENU_ZONE_W         90    // PigTeam : zone tactile (coin superieur gauche) qui ouvre le menu rapide : largeur (px)
#endif
#ifndef TS_MENU_ZONE_H
  #define TS_MENU_ZONE_H         70    // ... et hauteur (px)
#endif
#ifndef TS_LONGPRESS_MS
  #define TS_LONGPRESS_MS        1200  // PigTeam : appui long immobile = bascule radio <-> SD
#endif

#if TS_MODEL==TS_MODEL_XPT2046
  #include <XPT2046_Touchscreen.h>
  XPT2046_Touchscreen ts(TS_CS);
  typedef TS_Point TSPoint;
#elif TS_MODEL==TS_MODEL_GT911
 #include <TAMC_GT911.h>
  TAMC_GT911 ts = TAMC_GT911(TS_SDA, TS_SCL, TS_INT, TS_RST, 0, 0);
  typedef TP_Point TSPoint;
#elif TS_MODEL==TS_MODEL_FT6336
  #include "../libraries/FT6336_Touchscreen/FT6336.h"
  FT6336 ts = FT6336(TS_SDA, TS_SCL, TS_INT, TS_RST, 0, 0);
  typedef FT_Point TSPoint;
#endif

void TouchScreen::init(uint16_t w, uint16_t h) {
  
#if TS_MODEL==TS_MODEL_XPT2046
  #if defined(TS_SPI) && (TS_SPI == 'B') && defined(SPIB_SCK)
    ts.begin(SPIB);
  #elif defined(TS_SPI) && (TS_SPI == 'A')
    ts.begin(SPIA);
  #else
    ts.begin();
  #endif
  ts.setRotation(config.store.fliptouch?3:1);
#endif
#if TS_MODEL==TS_MODEL_GT911
  ts.begin();
  ts.setRotation(config.store.fliptouch?0:2);
#endif
#if TS_MODEL==TS_MODEL_FT6336
  ts.begin();
  ts.setRotation(config.store.fliptouch?2:0);
#endif
  _width  = w;
  _height = h;
#if TS_MODEL==TS_MODEL_GT911
  ts.setResolution(_width, _height);
#endif
#if TS_MODEL==TS_MODEL_FT6336
  ts.setResolution(_width, _height);
#endif
}

tsDirection_e TouchScreen::_tsDirection(uint16_t x, uint16_t y) {
  int16_t dX = x - _oldTouchX;
  int16_t dY = y - _oldTouchY;
  if (abs(dX) > TS_SWIPE_THRESHOLD_PX || abs(dY) > TS_SWIPE_THRESHOLD_PX) {
    if (abs(dX) > abs(dY)) {
      if (dX > 0) {
        return TSD_RIGHT;
      } else {
        return TSD_LEFT;
      }
    } else {
      if (dY > 0) {
        return TSD_DOWN;
      } else {
        return TSD_UP;
      }
    }
  } else {
    return TSD_REQUEST;
  }
}

void TouchScreen::flip() {
#if TS_MODEL==TS_MODEL_XPT2046
  ts.setRotation(config.store.fliptouch?3:1);
#endif
#if TS_MODEL==TS_MODEL_GT911
  ts.setRotation(config.store.fliptouch?0:2);
#endif
#if TS_MODEL==TS_MODEL_FT6336
  ts.setRotation(config.store.fliptouch?2:0);
#endif
}

void TouchScreen::loop() {
  uint16_t touchX, touchY;
  static bool wastouched = true;
  static uint32_t touchLongPress;
  static tsDirection_e direct;
  static uint16_t touchVol, touchStation;
  static bool cornerTap = false;       // PigTeam : toucher commence dans la zone du menu (coin superieur gauche)
  static bool slideMaybe = false;      // PigTeam : toucher commence pendant le diaporama (appui = stop, glissement vertical = luminosite)
  static bool brightChanged = false;   // PigTeam : luminosite modifiee par glissement vertical, a sauvegarder au relachement
  if (!_checklpdelay(20, _touchdelay)) return;
  #ifdef TOUCHMENU_ENABLE
    touchmenu.runPending();   // action choisie dans le menu, lancee une fois celui-ci ferme
  #endif
#if TS_MODEL==TS_MODEL_GT911
  ts.read();
#endif
#if TS_MODEL==TS_MODEL_FT6336
  ts.read();
#endif
  bool istouched = _istouched();
  if (istouched) {
    if (!_tapPending && _touchCooldown && (millis() - _touchCooldown < TS_COOLDOWN_MS)) return;
    _touchCooldown = 0;
  #if TS_MODEL==TS_MODEL_XPT2046
    TSPoint p = ts.getPoint();
    touchX = map(p.x, TS_X_MIN, TS_X_MAX, 0, _width);
    touchY = map(p.y, TS_Y_MIN, TS_Y_MAX, 0, _height);
  #elif TS_MODEL==TS_MODEL_GT911
    TSPoint p = ts.points[0];
    touchX = p.x;
    touchY = p.y;
  #elif TS_MODEL==TS_MODEL_FT6336
    TSPoint p = ts.points[0];
    touchX = p.x;
    touchY = p.y;
  #endif
  #ifdef TOUCHMENU_ENABLE
    if (touchmenu.active()) touchmenu.touchMove(touchX, touchY);
  #endif
  if (!wastouched) { /*     START TOUCH     */
      _tapPending = false;   // PigTeam : plus de double-tap (bascule SD = appui long)
      _isDoubleTap = false;
      _oldTouchX = touchX;
      _oldTouchY = touchY;
      touchVol = touchX;
      touchStation = touchY;
      direct = TSD_REQUEST;
      slideMaybe = false;
      touchLongPress=millis();
      #ifdef TOUCHMENU_ENABLE
        if (touchmenu.active()) {          // menu ouvert : les touchers lui appartiennent
          touchmenu.touchBegin();
          direct = TSD_STAY;
        }
      #endif
      #ifdef SLIDESHOW_ENABLE
        if (slideshow.active()) {          // un vrai appui arrete le diaporama (voir Slideshow::touchEnd) sans autre effet
          slideshow.touchBegin();
          direct = TSD_STAY;
          slideMaybe = true;
        }
      #endif
      #ifdef TOUCHMENU_ENABLE
        cornerTap = false;
        if (direct == TSD_REQUEST && display.mode()==PLAYER && touchX < TS_MENU_ZONE_W && touchY < TS_MENU_ZONE_H) {
          cornerTap = true;              // ouvre le menu au relachement (ni lecture/arret, ni glissement)
          direct = TSD_STAY;
        }
      #endif
    } else { /*     SWIPE TOUCH     */
      #ifdef TOUCHMENU_ENABLE
        if (cornerTap && (touchX >= TS_MENU_ZONE_W || touchY >= TS_MENU_ZONE_H)) cornerTap = false;   // le doigt a quitte la zone : annule
      #endif
      #ifdef SLIDESHOW_ENABLE
        if (slideMaybe && direct == TSD_STAY) {   // diaporama : un glissement regle luminosite / volume sans arreter le diaporama
          tsDirection_e d = _tsDirection(touchX, touchY);
          if (d != TSD_REQUEST) {
            direct = d;                           // glissement = reglage (haut/bas luminosite, gauche/droite volume), pas un arret
            slideshow.touchCancel();
            slideMaybe = false;
          }
        }
      #endif
      if (direct == TSD_REQUEST) {
        direct = _tsDirection(touchX, touchY);
        if (direct != TSD_REQUEST) {
          _tapPending = false;
          _isDoubleTap = false;
        }
      }
      if (direct == TSD_REQUEST && (millis() - touchLongPress >= TS_LONGPRESS_MS)) {
        #ifdef TOUCHMENU_ENABLE
          #ifdef TS_MENU_LONGPRESS
            if (display.mode()==PLAYER) touchmenu.open();   // menu rapide par appui long (desactive : on utilise la zone en haut a gauche)
          #endif
        #else
          controls.onBtnClick(EVT_BTN_MODE);   // radio <-> SD
        #endif
        direct = TSD_STAY;                   // ignore la suite du geste et le relachement
      }
      if (direct == TSD_REQUEST && (millis() - touchLongPress >= TS_DEEPSLEEP_MS)) {
        if (network.status != SDOFFLINE) {
          #ifndef DEEP_SLEEP_DISABLE
            display.putRequest(NEWMODE, SLEEPING);
          #endif
        }
      }
      switch (direct) {
        case TSD_LEFT:
        case TSD_RIGHT: {
            touchLongPress=millis();
            #ifdef SLIDESHOW_ENABLE
              if (slideshow.active()) {   // diaporama : volume direct, sans changer de page (la file d'affichage est videe a chaque tour)
                slideshow.touchCancel();
                int16_t xDeltaS = map(abs(touchVol - touchX), 0, _width, 0, TS_STEPS);
                if (xDeltaS>1) {
                  player.stepVol((touchVol - touchX)<0);
                  touchVol = touchX;
                }
                break;
              }
            #endif
            if (display.mode()==PLAYER || display.mode()==VOL) {
              int16_t xDelta = map(abs(touchVol - touchX), 0, _width, 0, TS_STEPS);
              display.putRequest(NEWMODE, VOL);
              if (xDelta>1) {
                controls.controlsEvent((touchVol - touchX)<0);
                touchVol = touchX;
              }
            }
            break;
          }
        case TSD_UP:
        case TSD_DOWN: {
            touchLongPress=millis();
            #ifdef SLIDESHOW_ENABLE
              if (slideshow.active()) slideshow.touchCancel();   // glissement en cours : ne pas arreter le diaporama
            #endif
            #ifdef TOUCHMENU_ENABLE
              // PigTeam : ancienne liste de stations au glissement vertical supprimee,
              // la selection se fait par le menu tactile (appui long > Stations).
              // Le glissement vertical regle maintenant la luminosite : haut = plus clair,
              // bas = plus sombre, toute la hauteur de l'ecran = 100 %.
              #if BRIGHTNESS_PIN!=255
                #ifdef SLIDESHOW_ENABLE
                  const bool slideOn = slideshow.active();
                #else
                  const bool slideOn = false;
                #endif
                if ((display.mode()==PLAYER || slideOn) && config.store.dspon) {
                  int16_t pct = ((int32_t)touchStation - (int32_t)touchY) * 100 / (int32_t)_height;
                  if (pct != 0) {
                    int16_t nb = (int16_t)config.store.brightness + pct;
                    if (nb < TS_BRIGHTNESS_MIN) nb = TS_BRIGHTNESS_MIN;
                    if (nb > 100) nb = 100;
                    touchStation = touchY;
                    if (nb != config.store.brightness) {
                      config.store.brightness = (uint8_t)nb;
                      config.setBrightness(false);   // applique tout de suite, sans ecrire en flash
                      brightChanged = true;
                    }
                  }
                }
              #endif
              break;
            #endif
            if (display.mode()==PLAYER) {
              // PigTeam : attendre que l'affichage soit vraiment passe en STATIONS avant de
              // bouger la liste (sinon controlsEvent voit encore le mode PLAYER = pas de volume)
              display.putRequest(NEWMODE, STATIONS);
              touchStation = touchY;
            } else if (display.mode()==STATIONS) {
              int16_t yDelta = map(abs(touchStation - touchY), 0, _height, 0, TS_STEPS);
              if (yDelta>1) {
                controls.controlsEvent((touchStation - touchY)<0);
                touchStation = touchY;
              }
            }
            break;
          }
        default:
            break;
      }
    }
    if (config.store.dbgtouch) {
      FUNCTIONLOG("Touch", "x = %d, y = %d", p.x, p.y);
    }
  } else {
    if (wastouched) {/*     END TOUCH     */
      #ifdef TOUCHMENU_ENABLE
        touchmenu.touchEnd();
      #endif
      #ifdef SLIDESHOW_ENABLE
        slideshow.touchEnd();
      #endif
      #ifdef TOUCHMENU_ENABLE
        if (cornerTap) {
          cornerTap = false;
          if (millis() - touchLongPress >= 50 && display.mode()==PLAYER) touchmenu.open();   // appui franc dans la zone : menu rapide
        }
      #endif
      #if BRIGHTNESS_PIN!=255
        if (brightChanged) {
          brightChanged = false;
          config.setBrightness(true);   // sauvegarde (differee) de la nouvelle luminosite
          #if DSP_DIMMING_ENABLED
            backlightControls.restart();  // relance le delai d'attenuation
          #endif
        }
      #endif
      if (direct == TSD_REQUEST) {
        uint32_t pressTicks = millis()-touchLongPress;
        if (pressTicks >= TS_DEEPSLEEP_MS) {
          #ifndef DEEP_SLEEP_DISABLE
            utility.doSleepW();
          #endif
        } else if (pressTicks > 50) {
          controls.onBtnClick(EVT_BTN_PLAY);   // PigTeam : toucher simple = lecture / arret, sans delai
        }
      }
      direct = TSD_STAY;
      _touchCooldown = millis();
    }
    if (_tapPending && (millis() - _tapPendingTime >= TS_DOUBLETAP_MS)) {
      controls.onBtnClick(EVT_BTN_PLAY);
      _tapPending = false;
    }
  }
  wastouched = istouched;
}

bool TouchScreen::_checklpdelay(int m, uint32_t &tstamp) {
  if (millis() - tstamp > m) {
    tstamp = millis();
    return true;
  } else {
    return false;
  }
}

bool TouchScreen::_istouched() {
#if TS_MODEL==TS_MODEL_XPT2046
  return ts.touched();
#elif TS_MODEL==TS_MODEL_GT911
  return ts.isTouched;
#elif TS_MODEL==TS_MODEL_FT6336
  return ts.isTouched;
#endif
}

#endif  // TS_MODEL!=TS_MODEL_UNDEFINED
