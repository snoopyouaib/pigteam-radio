#ifndef touchmenu_h
#define touchmenu_h
#include "options.h"

/* Menu rapide tactile : un appui long (sur l'ecran du lecteur) ouvre une grille de gros boutons :
   4 colonnes x 2 lignes : Diapo (sous-menu ON / Duree / Aleatoire / Normal), Ecran off,
   Mode SD/Radio, Reveil on/off, Arret programme, Stations (liste a pages), Fermer, + 1 pave libre.
   Active par  #define TOUCHMENU_ENABLE  dans myoptions.h (ecran couleur + tactile requis). */
#if defined(TOUCHMENU_ENABLE) && (defined(DUMMYDISPLAY) || defined(DSP_OLED) || defined(DSP_LCD) || TS_MODEL==TS_MODEL_UNDEFINED)
  #undef TOUCHMENU_ENABLE
#endif

#ifdef TOUCHMENU_ENABLE
#include <Arduino.h>

#ifndef TOUCHMENU_TIMEOUT_MS
  #define TOUCHMENU_TIMEOUT_MS 10000     // fermeture automatique apres inactivite
#endif
#ifndef TOUCHMENU_MIN_PRESS_MS
  #define TOUCHMENU_MIN_PRESS_MS 60      // appui minimal pour valider (filtre les touchers parasites)
#endif

enum touchMenuEvent_e : uint8_t { TME_NONE, TME_OPENED, TME_RUNNING, TME_CLOSED };

class TouchMenu {
  public:
    void open();                              // tache tactile
    void close() { _wantOn = false; }
    bool active() const { return _wantOn; }
    void touchBegin();                        // tache tactile
    void touchMove(uint16_t x, uint16_t y);
    void touchEnd();
    void runPending();                        // tache tactile : lance l'action choisie une fois le menu ferme
    void closedDone() { _closedDone = true; } // tache d'affichage : ecran du lecteur restaure
    touchMenuEvent_e poll();                  // tache d'affichage
    void step();
  private:
    volatile bool _wantOn = false;
    bool _on = false;
    volatile bool _armed = false;             // faux jusqu'au relachement du toucher qui a ouvert le menu
    volatile bool _down = false;
    volatile bool _busy = false;              // dessin en cours
    volatile bool _dirty = false;
    volatile bool _closedDone = false;
    volatile int8_t _pending = -1;            // action differee (apres fermeture)
    volatile uint16_t _x = 0, _y = 0;
    uint32_t _t0 = 0;
    volatile uint32_t _lastAct = 0;
    volatile uint8_t _page = 0;               // 0 = menu principal, 1 = sous-menu diaporama, 2 = liste des stations
    volatile uint16_t _stPage = 0;            // page courante de la liste des stations (0 = premiere)
    volatile uint16_t _slideSecs = 5;         // duree par photo choisie dans le sous-menu diaporama (secondes)
    volatile uint16_t _arg = 0;               // argument de l'action differee (numero de station)
    uint8_t _drawPage = 0;                    // page en cours de dessin (copie coherente pour toute la passe)
    int _hit(int x, int y);
    void _draw();
    void _drawTile(int i, int W, int H);
};

extern TouchMenu touchmenu;
#endif // TOUCHMENU_ENABLE
#endif // touchmenu_h
