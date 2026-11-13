#ifndef sdbackup_h
#define sdbackup_h
#include <Arduino.h>

/* Sauvegarde / restauration de TOUS les fichiers de la memoire flash (SPIFFS : /www, /data, playlist,
   wifi, pense-bete...) vers le dossier /backup de la carte SD, et inversement.
   Lance depuis la page web (/memo.html) ou par commande (sdbackup=1 / sdrestore=1). Travaille dans une tache
   a part ; la radio redemarre toute seule apres une restauration reussie. */
class SdBackup {
  public:
    bool   start(bool restore);     // faux si une operation est deja en cours
    bool   busy() const { return _busy; }
    String statusJson();
  private:
    volatile bool _busy = false;
    volatile bool _ok = true;
    String _msg;
    bool _restore = false;
    static void _task(void* arg);
    void _run();
    void _finish(bool ok, const String& msg);
};

extern SdBackup sdbackup;
#endif
