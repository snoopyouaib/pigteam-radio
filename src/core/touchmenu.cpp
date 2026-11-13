#include "touchmenu.h"

#ifdef TOUCHMENU_ENABLE
#include "config.h"
#include "controls.h"
#include "player.h"
#include "utility.h"
#include "slideshow.h"
#include "netserver.h"
#include "network.h"
#include "display.h"
#include "backlightcontrols.h"
#include "../displays/dspcore.h"

TouchMenu touchmenu;

/* menu principal : 4 colonnes x 2 lignes, dans l'ordre de lecture */
enum { TL_SLIDE, TL_SCREEN, TL_MODE, TL_ALARM, TL_SLEEP, TL_STATIONS, TL_FREE, TL_CLOSE, TL_COUNT };
/* sous-menu diaporama : 2 colonnes x 2 lignes (SB_TIME = duree par photo, chaque toucher passe a la suivante) */
enum { SB_ON, SB_TIME, SB_RANDOM, SB_NORMAL, SB_COUNT };
/* sous-menu source : 2 colonnes x 2 lignes (Radio / SD normal / SD aleatoire / Retour) */
enum { SRC_RADIO, SRC_SD, SRC_SDRND, SRC_BACK, SRC_COUNT };
static const uint16_t SLIDE_TIMES[] = { 5, 8, 15, 30, 60 };   // secondes par photo
#define SLIDE_TIMES_N (sizeof(SLIDE_TIMES) / sizeof(SLIDE_TIMES[0]))
/* liste des stations : 6 par page (2 colonnes x 3 lignes) + ligne de navigation  < Retour > */
enum { ST_PER_PAGE = 6, ST_PREV = 6, ST_BACK = 7, ST_NEXT = 8, ST_COUNT = 9 };

#define TM_GAP    8
#define TM_BG     0x0000
#define TM_TILE   0x18E3
#define TM_EDGE   0x8410
#define TM_TEXT   0xFFFF
#define TM_OFF    0x7BCF
#define TM_ON     0x07E0
#define TM_WARM   0xFD20

static void tileRect(int page, int i, int W, int H, int& x, int& y, int& w, int& h) {
  if (page == 2) {                           // 3 lignes de 2 stations + ligne de navigation (3 paves)
    h = (H - 5 * TM_GAP) / 4;
    if (i < ST_PER_PAGE) {
      w = (W - 3 * TM_GAP) / 2;
      x = TM_GAP + (i % 2) * (w + TM_GAP);
      y = TM_GAP + (i / 2) * (h + TM_GAP);
    } else {
      w = (W - 4 * TM_GAP) / 3;
      x = TM_GAP + (i - ST_PER_PAGE) * (w + TM_GAP);
      y = TM_GAP + 3 * (h + TM_GAP);
    }
    return;
  }
  const int cols = (page == 0) ? 4 : 2;      // menu principal 4 x 2 ; sous-menu diaporama 2 x 2 (gros paves)
  w = (W - (cols + 1) * TM_GAP) / cols;
  h = (H - 3 * TM_GAP) / 2;
  x = TM_GAP + (i % cols) * (w + TM_GAP);
  y = TM_GAP + (i / cols) * (h + TM_GAP);
}

/* plus grande taille de texte (police 6x8 grossie) <= pref qui tient dans maxw pixels */
static int fitSize(const char* s, int pref, int maxw) {
  while (pref > 1 && (int)strlen(s) * 6 * pref > maxw) pref--;
  return pref;
}

static bool tileEnabled(int i) {
  switch (i) {
    case TL_FREE: return false;        // reserve pour une fonction future
    #ifndef SLIDESHOW_ENABLE
      case TL_SLIDE: return false;
    #endif
    #ifndef ALARM_ENABLE
      case TL_ALARM: return false;
    #endif
    #ifndef SLEEP_TIMER_ENABLE
      case TL_SLEEP: return false;
    #endif
    default: return true;
  }
}

void TouchMenu::open() {
  if (_wantOn || _on) return;
  _armed = false;
  _down = false;
  _page = 0;
  _stPage = 0;
  _pending = -1;
  _closedDone = false;
  _lastAct = millis();
  _wantOn = true;
}

touchMenuEvent_e TouchMenu::poll() {
  if (_wantOn && !_on) { _on = true; _dirty = true; return TME_OPENED; }
  if (_wantOn && _on) return TME_RUNNING;
  if (!_wantOn && _on) { _on = false; return TME_CLOSED; }
  return TME_NONE;
}

void TouchMenu::step() {
  if (!_wantOn) return;
  if (millis() - _lastAct > TOUCHMENU_TIMEOUT_MS) { _wantOn = false; return; }
  if (_dirty) _draw();
}

int TouchMenu::_hit(int x, int y) {
  const int W = dsp.width(), H = dsp.height();
  const int count = (_page == 0) ? (int)TL_COUNT : (_page == 1) ? (int)SB_COUNT : (_page == 3) ? (int)SRC_COUNT : (int)ST_COUNT;
  for (int i = 0; i < count; i++) {
    int tx, ty, tw, th;
    tileRect(_page, i, W, H, tx, ty, tw, th);
    if (x >= tx && x < tx + tw && y >= ty && y < ty + th) return i;
  }
  return -1;   // entre deux boutons : ignore
}

void TouchMenu::touchBegin() {
  if (!_wantOn) return;
  _lastAct = millis();
  if (!_armed) return;
  if (_busy) { _down = false; return; }   // le tactile partage le bus avec l'ecran : on ignore ce qui commence pendant le dessin
  _down = true;
  _t0 = millis();
}

void TouchMenu::touchMove(uint16_t x, uint16_t y) {
  if (!_wantOn) return;
  _x = x;
  _y = y;
  _lastAct = millis();
}

void TouchMenu::touchEnd() {
  if (!_wantOn) return;
  _lastAct = millis();
  if (!_armed) { _armed = true; _down = false; return; }   // relachement du toucher qui a ouvert le menu
  if (!_down) return;
  _down = false;
  if (millis() - _t0 < TOUCHMENU_MIN_PRESS_MS) return;
  const int hit = _hit(_x, _y);
  if (_page == 2) {                  // liste des stations
    const uint16_t total = utility.playlistLength();
    const uint16_t pages = (total + ST_PER_PAGE - 1) / ST_PER_PAGE;
    if (hit >= 0 && hit < ST_PER_PAGE) {
      uint16_t id = _stPage * ST_PER_PAGE + hit + 1;
      if (id <= total) { _arg = id; _pending = TL_STATIONS; _wantOn = false; }   // lance apres fermeture du menu
    } else if (hit == ST_PREV && pages > 1) {
      _stPage = (_stPage == 0) ? pages - 1 : _stPage - 1;
      _dirty = true;
    } else if (hit == ST_NEXT && pages > 1) {
      _stPage = (_stPage + 1 >= pages) ? 0 : _stPage + 1;
      _dirty = true;
    } else if (hit == ST_BACK) {
      _page = 0;
      _dirty = true;
    }
    return;
  }
  if (_page == 3) {                  // sous-menu source : Radio / SD normal / SD aleatoire
    switch (hit) {
      case SRC_RADIO: if (network.status != SDOFFLINE) { _arg = 0; _pending = TL_MODE; _wantOn = false; } break;   // lance apres fermeture du menu
      case SRC_SD:    _arg = 1; _pending = TL_MODE; _wantOn = false; break;
      case SRC_SDRND: _arg = 2; _pending = TL_MODE; _wantOn = false; break;
      case SRC_BACK:  _page = 0; _dirty = true; break;
      default: break;
    }
    return;
  }
  if (_page == 1) {                  // sous-menu diaporama
    switch (hit) {
      #ifdef SLIDESHOW_ENABLE
      case SB_ON:     _pending = TL_SLIDE; _wantOn = false; break;                    // lance apres fermeture du menu
      case SB_TIME: {                                                                 // duree par photo : 5 -> 8 -> 15 -> 30 -> 60 -> 5 ...
        size_t k = 0;
        for (size_t j = 0; j < SLIDE_TIMES_N; j++) if (SLIDE_TIMES[j] == _slideSecs) k = j + 1;
        _slideSecs = SLIDE_TIMES[k % SLIDE_TIMES_N];
        _dirty = true;
        break;
      }
      case SB_RANDOM: slideshow.setRandom(true);  netserver.requestOnChange(SLIDESHOW, 0); _dirty = true; break;
      case SB_NORMAL: slideshow.setRandom(false); netserver.requestOnChange(SLIDESHOW, 0); _dirty = true; break;
      #endif
      default: break;
    }
    return;
  }
  switch (hit) {                     // menu principal
    case TL_SLIDE:  if (tileEnabled(TL_SLIDE)) { _page = 1; _dirty = true; } break;
    case TL_STATIONS:
      if (config.getMode() == PM_WEB && utility.playlistLength() > 0) {   // liste des stations (radio web seulement)
        uint16_t cur = config.lastStation();
        _stPage = (cur > 0) ? (cur - 1) / ST_PER_PAGE : 0;                  // ouvre sur la page de la station en cours
        _page = 2;
        _dirty = true;
      }
      break;
    case TL_SCREEN: _pending = TL_SCREEN; _wantOn = false; break;
    case TL_MODE:   _page = 3; _dirty = true; break;   // sous-menu source (Radio / SD / SD aleatoire)
    case TL_CLOSE:  _wantOn = false; break;
    #ifdef ALARM_ENABLE
    case TL_ALARM:
      config.saveValue(&config.store.alarmon, !config.store.alarmon);
      netserver.requestOnChange(ALARM, 0);
      _dirty = true;
      break;
    #endif
    #ifdef SLEEP_TIMER_ENABLE
    case TL_SLEEP: {
      uint16_t m = player.sleepMinutes();
      uint16_t n = (m == 0) ? 15 : (m <= 15) ? 30 : (m <= 30) ? 60 : (m <= 60) ? 90 : 0;
      player.startSleepTimer(n);
      _dirty = true;
      break;
    }
    #endif
    default: break;
  }
}

/* Actions qui touchent a l'affichage ou aux modes : lancees apres la fermeture du menu,
   depuis la tache tactile (comme l'ancien appui long radio <-> SD), jamais depuis la tache d'affichage. */
void TouchMenu::runPending() {
  if (_pending < 0 || !_closedDone) return;
  int a = _pending;
  _pending = -1;
  switch (a) {
    #ifdef SLIDESHOW_ENABLE
    case TL_SLIDE:  slideshow.start(_slideSecs); break;
    #endif
    case TL_SCREEN: config.setDspOn(false); backlightControls.restart(); break;
    case TL_STATIONS: player.sendCommand({PR_PLAY, (uint32_t)_arg}); break;
    case TL_MODE: {                                    // _arg : 0 = radio, 1 = SD normal, 2 = SD aleatoire
      const bool web = (config.getMode() == PM_WEB);
      if (_arg == 0) {
        if (!web) controls.onBtnClick(EVT_BTN_MODE);   // SD -> radio
      } else {
        config.saveValue(&config.store.sdshuffle, (bool)(_arg == 2));
        netserver.requestOnChange(SDSHUFFLE, 0);
        display.putRequest(DSPRSSI, 0);
        if (web) controls.onBtnClick(EVT_BTN_MODE);    // radio -> SD (l'ordre choisi est deja enregistre)
        else if (_arg == 2) player.next();             // deja en SD : tire tout de suite une autre piste
      }
      break;
    }
    default: break;
  }
}

void TouchMenu::_drawTile(int i, int W, int H) {
  const int pg = _drawPage;
  int x, y, w, h;
  tileRect(pg, i, W, H, x, y, w, h);
  bool en = true;
  const char* l1 = "";
  const char* l2 = "";
  char buf[48];
  char nm[48];
  uint16_t edge = TM_EDGE;
  int pref1 = (pg == 0) ? 2 : 3;
  if (pg == 0) {
    en = tileEnabled(i);
    switch (i) {
      case TL_SLIDE:
        l1 = "Diapo";
        #ifdef SLIDESHOW_ENABLE
          l2 = slideshow.isRandom() ? "melange" : "normal";
        #endif
        break;
      case TL_SCREEN: l1 = "Ecran"; l2 = "eteindre"; break;
      case TL_MODE:
        l1 = "Source";
        l2 = (config.getMode() == PM_WEB) ? "Radio" : config.store.sdshuffle ? "SD melange" : "SD normal";
        break;
      case TL_ALARM:
        l1 = "Reveil";
        #ifdef ALARM_ENABLE
          l2 = config.store.alarmon ? "ON" : "OFF";
          if (config.store.alarmon) edge = TM_ON;
        #endif
        break;
      case TL_SLEEP:
        l1 = "Arret";
        l2 = "non";
        #ifdef SLEEP_TIMER_ENABLE
          if (player.sleepMinutes()) {                         // minutes RESTANTES (arrondies au-dessus), pas la duree choisie
            snprintf(buf, sizeof(buf), "%u min", (unsigned)((player.sleepRemaining() + 59) / 60));
            l2 = buf; edge = TM_WARM;
          }
        #endif
        break;
      case TL_STATIONS:
        l1 = "Stations";
        l2 = "choisir";
        en = (config.getMode() == PM_WEB);
        break;
      case TL_CLOSE:  l1 = "Fermer"; break;
      default:        l1 = "---"; break;
    }
  } else if (pg == 1) {
    #ifdef SLIDESHOW_ENABLE
      const bool rnd = slideshow.isRandom();
    #else
      const bool rnd = false;
    #endif
    switch (i) {
      case SB_ON:     l1 = "ON";        l2 = "lancer"; edge = TM_ON; break;
      case SB_TIME:   l1 = "Duree";     snprintf(buf, sizeof(buf), "%u s / photo", (unsigned)_slideSecs); l2 = buf; edge = TM_WARM; break;
      case SB_RANDOM: l1 = "Aleatoire"; l2 = "ordre melange"; if (rnd) edge = TM_ON; break;
      case SB_NORMAL: l1 = "Normal";    l2 = "alphabetique";  if (!rnd) edge = TM_ON; break;
      default: break;
    }
  } else if (pg == 3) {                      // sous-menu source
    const bool web = (config.getMode() == PM_WEB);
    switch (i) {
      case SRC_RADIO: l1 = "Radio"; l2 = "web"; en = (network.status != SDOFFLINE); if (web) edge = TM_ON; break;
      case SRC_SD:    l1 = "SD";    l2 = "normal"; if (!web && !config.store.sdshuffle) edge = TM_ON; break;
      case SRC_SDRND: l1 = "SD";    l2 = "aleatoire"; if (!web && config.store.sdshuffle) edge = TM_ON; break;
      default:        l1 = "Retour"; break;
    }
  } else {                                   // liste des stations
    const uint16_t total = utility.playlistLength();
    const uint16_t pages = (total + ST_PER_PAGE - 1) / ST_PER_PAGE;
    pref1 = 2;
    if (i < ST_PER_PAGE) {
      const uint16_t id = _stPage * ST_PER_PAGE + i + 1;
      if (id <= total) {
        char name[40];
        strlcpy(name, utility.stationByNum(id), sizeof(name));   // copie tout de suite : tampon partage
        snprintf(nm, sizeof(nm), "%u %s", (unsigned)id, name);
        const int maxc = (w - 8) / 12;                         // nombre de caracteres qui tiennent a la taille 2
        if ((int)strlen(nm) > maxc) nm[maxc] = 0;
        l1 = nm;
        if (id == config.lastStation()) edge = TM_ON;          // station en cours
      } else {
        l1 = "";
        en = false;
      }
    } else if (i == ST_PREV) { l1 = "<"; pref1 = 3; en = pages > 1; }
    else if (i == ST_NEXT)   { l1 = ">"; pref1 = 3; en = pages > 1; }
    else {
      l1 = "Retour";
      snprintf(buf, sizeof(buf), "%u/%u", (unsigned)(_stPage + 1), (unsigned)(pages ? pages : 1));
      l2 = buf;
    }
  }
  dsp.fillRoundRect(x, y, w, h, 10, TM_TILE);
  dsp.drawRoundRect(x, y, w, h, 10, en ? edge : TM_OFF);
  if (en && edge == TM_ON && ((pg == 1 && (i == SB_RANDOM || i == SB_NORMAL)) || (pg == 2 && i < ST_PER_PAGE) || (pg == 3 && i < SRC_BACK)))
    dsp.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 9, TM_ON);   // choix / station actifs : trait epais
  dsp.setFont();
  dsp.setTextWrap(false);
  dsp.setTextColor(en ? TM_TEXT : TM_OFF);
  const int s1 = fitSize(l1, pref1, w - 8);   // taille du texte (police 6x8 grossie), ajustee a la largeur du pave
  const int s2 = fitSize(l2, 2, w - 8);
  const int h1 = 8 * s1, h2 = 8 * s2;
  const bool two = l2[0] != 0;
  const int top = two ? y + (h - (h1 + 6 + h2)) / 2 : y + (h - h1) / 2;
  dsp.setTextSize(s1);
  dsp.setCursor(x + (w - (int)strlen(l1) * 6 * s1) / 2, top);
  dsp.print(l1);
  if (two) {
    dsp.setTextSize(s2);
    dsp.setCursor(x + (w - (int)strlen(l2) * 6 * s2) / 2, top + h1 + 6);
    dsp.print(l2);
  }
}

void TouchMenu::_draw() {
  _busy = true;
  _dirty = false;
  _drawPage = _page;
  const int W = dsp.width(), H = dsp.height();
  dsp.fillScreen(TM_BG);
  const int count = (_drawPage == 0) ? (int)TL_COUNT : (_drawPage == 1) ? (int)SB_COUNT : (_drawPage == 3) ? (int)SRC_COUNT : (int)ST_COUNT;
  for (int i = 0; i < count; i++) _drawTile(i, W, H);
  _busy = false;
}
#endif // TOUCHMENU_ENABLE
