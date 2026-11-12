#include "slideshow.h"

#ifdef SLIDESHOW_ENABLE
#include <algorithm>
#include <FS.h>
#include <SD.h>
#include <JPEGDEC.h>
#include <esp_random.h>
#include "config.h"
#include "logging.h"
#include "sdmanager.h"
#include "../displays/dspcore.h"

Slideshow slideshow;

static JPEGDEC jpeg;
static File ssFile;   // une seule photo ouverte a la fois

/* --- acces fichier SD pour JPEGDEC --- */
static void* ssOpen(const char* name, int32_t* size) {
  ssFile = sdman.open(name, FILE_READ);
  if (!ssFile) return nullptr;
  *size = (int32_t)ssFile.size();
  return &ssFile;
}
static void ssClose(void*) { if (ssFile) ssFile.close(); }
static int32_t ssRead(JPEGFILE*, uint8_t* buf, int32_t len) { return ssFile ? (int32_t)ssFile.read(buf, len) : 0; }
static int32_t ssSeek(JPEGFILE*, int32_t pos) { return ssFile ? (int32_t)ssFile.seek(pos) : 0; }

/* --- un bloc decode (MCU) -> ecran ; l'ecran est dans dsp, rotation deja appliquee --- */
static int ssDraw(JPEGDRAW* d) {
  if (!slideshow.active()) return 0;   // arret demande : on abandonne le decodage en cours
  if (d->iWidthUsed >= d->iWidth) {
    dsp.drawRGBBitmap(d->x, d->y, d->pPixels, d->iWidth, d->iHeight);
  } else {                             // bloc de bord : on ne garde que les colonnes utiles
    for (int r = 0; r < d->iHeight; r++) dsp.drawRGBBitmap(d->x, d->y + r, d->pPixels + (size_t)r * d->iWidth, d->iWidthUsed, 1);
  }
  return 1;
}

void Slideshow::start(uint16_t secs) {
  if (secs < 1) secs = SLIDESHOW_DEFAULT_SECS;
  _secs = secs;
  _wantOn = true;
}

void Slideshow::stop(const char* who) {
  if (_wantOn) FUNCTIONLOG("Slideshow", "arret demande par : %s", who);
  _wantOn = false;
}

/* Le tactile XPT2046 partage le bus SPI avec l'ecran : pendant qu'une photo s'ecrit, il peut renvoyer
   des touchers fantomes. On ignore donc les touchers qui commencent pendant l'ecriture, et on ne
   s'arrete qu'a la fin d'un appui qui a dure au moins 60 ms (un parasite dure un seul cycle de 20 ms). */
void Slideshow::touchBegin() {
  if (_swipeAt && (millis() - _swipeAt) < 800) { _touchAt = 0; return; }   // suite d'un glissement coupe par le tactile : pas un appui
  _touchAt = _busy ? 0 : millis();
  if (!_busy && _touchAt == 0) _touchAt = 1;
}

void Slideshow::touchCancel() {
  _touchAt = 0;
  _swipeAt = millis();
  if (_swipeAt == 0) _swipeAt = 1;
}

void Slideshow::touchEnd() {
  if (_touchAt && !_busy && (millis() - _touchAt) >= 60) stop("toucher");
  _touchAt = 0;
}

slideshowEvent_e Slideshow::poll() {
  if (_wantOn && !_on) {
    _on = true;
    _files.clear();
    _order.clear();
    _last = -1;
    _idx = 0;
    _failures = 0;
    _shown = false;
    _nextAt = 0;
    return SSE_STARTED;
  }
  if (_wantOn && _on) return SSE_RUNNING;
  if (!_wantOn && _on) {
    _on = false;
    _files.clear();
    _files.shrink_to_fit();
    _order.clear();
    _order.shrink_to_fit();
    if (_mountedByUs) {                // en mode radio web la SD est normalement demontee : on remet comme avant
      if (config.getMode() == PM_WEB) sdman.stop();
      _mountedByUs = false;
    }
    return SSE_STOPPED;
  }
  return SSE_NONE;
}

void Slideshow::_fail(const char* msg) {
  FUNCTIONLOG("Slideshow", "%s", msg);
  config.setTitle(msg);   // visible sur l'ecran / l'interface une fois la radio reaffichee
  _wantOn = false;
}

bool Slideshow::_scan() {
  if (!sdman.ready) {                  // en mode radio web la SD n'est pas montee : on la monte pour le diaporama
    if (!sdman.start()) { sdman.stop(); _fail("Diaporama : carte SD absente"); return false; }
    _mountedByUs = true;
  }
  File dir = sdman.open(SLIDESHOW_DIR);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    _fail("Diaporama : dossier " SLIDESHOW_DIR " introuvable");
    return false;
  }
  while (true) {
    File f = dir.openNextFile();
    if (!f) break;
    bool isDir = f.isDirectory();
    String n = f.name();
    f.close();
    if (isDir) continue;
    int slash = n.lastIndexOf('/');
    if (slash >= 0) n = n.substring(slash + 1);
    if (n.length() == 0 || n[0] == '.') continue;   // fichiers caches (ex. ._IMG.jpg de macOS)
    String l = n;
    l.toLowerCase();
    if (!(l.endsWith(".jpg") || l.endsWith(".jpeg"))) continue;
    _files.push_back(String(SLIDESHOW_DIR) + "/" + n);
    if (_files.size() >= SLIDESHOW_MAX_FILES) break;
  }
  dir.close();
  if (_files.empty()) { _fail("Diaporama : aucune photo dans " SLIDESHOW_DIR); return false; }
  std::sort(_files.begin(), _files.end(), [](const String& a, const String& b) { return a.compareTo(b) < 0; });
  FUNCTIONLOG("Slideshow", "%u photo(s) dans %s", (unsigned)_files.size(), SLIDESHOW_DIR);
  return true;
}

bool Slideshow::_show(const char* path) {
  uint32_t t0 = millis();
  if (!jpeg.open(path, ssOpen, ssClose, ssRead, ssSeek, ssDraw)) {
    FUNCTIONLOG("Slideshow", "ouverture impossible : %s (erreur %d)", path, jpeg.getLastError());
    return false;
  }
  const int W = dsp.width(), H = dsp.height();
  const int iw = jpeg.getWidth(), ih = jpeg.getHeight();
  // plus petite reduction (1, 1/2, 1/4, 1/8) qui fait entrer la photo dans l'ecran ; sinon 1/8 et recadrage centre
  int s = 1, opt = 0;
  for (; s <= 8; s <<= 1) {
    opt = (s == 1) ? 0 : (s == 2) ? JPEG_SCALE_HALF : (s == 4) ? JPEG_SCALE_QUARTER : JPEG_SCALE_EIGHTH;
    if ((iw + s - 1) / s <= W && (ih + s - 1) / s <= H) break;
  }
  if (s > 8) { s = 8; opt = JPEG_SCALE_EIGHTH; }
  const int cw = (iw + s - 1) / s, ch = (ih + s - 1) / s;
  const int ox = (W - cw) / 2, oy = (H - ch) / 2;
  // bandes noires autour de la photo (la photo elle-meme est entierement reecrite)
  if (ox > 0) { dsp.fillRect(0, 0, ox, H, 0); dsp.fillRect(ox + cw, 0, W - ox - cw, H, 0); }
  if (oy > 0) { dsp.fillRect(0, 0, W, oy, 0); dsp.fillRect(0, oy + ch, W, H - oy - ch, 0); }
  _busy = true;
  int ok = jpeg.decode(ox, oy, opt);
  int err = jpeg.getLastError();
  jpeg.close();
  _busy = false;
  FUNCTIONLOG("Slideshow", "%s %dx%d /%d -> %lu ms%s", path, iw, ih, s, (unsigned long)(millis() - t0), ok ? "" : " (ECHEC)");
  if (!ok && slideshow.active()) FUNCTIONLOG("Slideshow", "erreur decodage %d (JPEG progressif non supporte ?)", err);
  return ok != 0 || !slideshow.active();   // un arret demande pendant le decodage n'est pas une erreur
}

/* Ordre de passage : alphabetique, ou melange (Fisher-Yates) ; en melange chaque photo passe une fois par tour */
void Slideshow::_buildOrder() {
  const size_t n = _files.size();
  _order.resize(n);
  for (size_t i = 0; i < n; i++) _order[i] = (uint16_t)i;
  if (_random && n > 1) {
    for (size_t i = n - 1; i > 0; i--) std::swap(_order[i], _order[esp_random() % (i + 1)]);
    if (_last >= 0 && (int32_t)_order[0] == _last) std::swap(_order[0], _order[1]);   // pas deux fois la meme de suite
  }
  _idx = 0;
  _reorder = false;
}

void Slideshow::step() {
  if (!_wantOn) return;
  if (_nextAt && (int32_t)(millis() - _nextAt) < 0) return;
  if (_files.empty() && !_scan()) return;
  if (_files.size() == 1 && _shown) { _nextAt = millis() + (uint32_t)_secs * 1000UL; return; }   // une seule photo : pas la peine de la redecoder
  if (_reorder || _order.size() != _files.size() || _idx >= _order.size()) _buildOrder();
  const size_t cur = _order[_idx++];
  _last = (int32_t)cur;
  bool ok = _show(_files[cur].c_str());
  if (!_wantOn) return;
  if (ok) {
    _failures = 0;
    _shown = true;
  } else if (++_failures >= _files.size()) {
    _fail("Diaporama : photos illisibles (JPEG baseline requis)");
    return;
  }
  _nextAt = millis() + (ok ? (uint32_t)_secs * 1000UL : 50UL);
  if (_nextAt == 0) _nextAt = 1;
}
#endif // SLIDESHOW_ENABLE
