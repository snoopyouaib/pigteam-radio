#ifndef slideshow_h
#define slideshow_h
#include "options.h"

/* Diaporama de photos JPEG stockees sur la carte SD (dossier SLIDESHOW_DIR).
   Active par  #define SLIDESHOW_ENABLE  dans myoptions.h, avec la lib JPEGDEC dans lib_deps.
   Desactive automatiquement si la lib est absente, sans ecran couleur, ou sans ecran. */
#if defined(SLIDESHOW_ENABLE) && (defined(DUMMYDISPLAY) || defined(DSP_OLED) || defined(DSP_LCD) || SD_CS==255 || !__has_include(<JPEGDEC.h>))
  #undef SLIDESHOW_ENABLE
#endif

#ifdef SLIDESHOW_ENABLE
#include <Arduino.h>
#include <vector>

#ifndef SLIDESHOW_DIR
  #define SLIDESHOW_DIR "/photos"        // dossier des photos sur la SD
#endif
#ifndef SLIDESHOW_DEFAULT_SECS
  #define SLIDESHOW_DEFAULT_SECS 8       // duree d'affichage par defaut (s)
#endif
#ifndef SLIDESHOW_RANDOM_DEFAULT
  #define SLIDESHOW_RANDOM_DEFAULT false // ordre aleatoire par defaut (modifiable depuis l'interface web)
#endif
#ifndef SLIDESHOW_MAX_FILES
  #define SLIDESHOW_MAX_FILES 500        // nombre maxi de photos listees
#endif

enum slideshowEvent_e : uint8_t { SSE_NONE, SSE_STARTED, SSE_RUNNING, SSE_STOPPED };

class Slideshow {
  public:
    void start(uint16_t secs = SLIDESHOW_DEFAULT_SECS);   // depuis n'importe quelle tache
    void stop(const char* who = "?");                     // idem ; 'who' apparait dans le log
    void touchBegin();                                    // tactile : debut d'un toucher
    void touchEnd();                                      // tactile : fin d'un toucher (arrete si c'etait un vrai appui)
    bool active() const { return _wantOn; }
    void setRandom(bool r) { _random = r; _reorder = true; }
    bool isRandom() const { return _random; }
    uint16_t seconds() const { return _wantOn ? _secs : 0; }
    // appelees uniquement par la tache d'affichage
    slideshowEvent_e poll();
    void step();
  private:
    volatile bool _wantOn = false;
    bool _on = false;
    volatile uint16_t _secs = SLIDESHOW_DEFAULT_SECS;
    uint32_t _nextAt = 0;
    size_t _idx = 0;
    uint16_t _failures = 0;
    bool _shown = false;
    bool _mountedByUs = false;
    volatile bool _random = SLIDESHOW_RANDOM_DEFAULT;
    volatile bool _reorder = false;
    std::vector<uint16_t> _order;     // ordre de passage (indices dans _files)
    int32_t _last = -1;               // derniere photo affichee (evite de la rejouer en tete du tour suivant)
    void _buildOrder();
    volatile bool _busy = false;      // decodage / ecriture ecran en cours
    uint32_t _touchAt = 0;
    std::vector<String> _files;
    bool _scan();
    bool _show(const char* path);
    void _fail(const char* msg);
};

extern Slideshow slideshow;
#endif // SLIDESHOW_ENABLE
#endif // slideshow_h
