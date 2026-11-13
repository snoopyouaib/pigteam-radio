#include "sdbackup.h"
#include "options.h"
#ifdef USE_SD
#include <FS.h>
#include <SD.h>
#include <SPIFFS.h>
#include <vector>
#include "config.h"
#include "player.h"
#include "sdmanager.h"
#include "slideshow.h"
#include "memo.h"
#include "logging.h"

#define BACKUP_DIR "/backup"

struct Entry { String path; bool isDir; size_t size; };

/* liste (recursive) d'un dossier ; la liste est lue en entier puis le dossier est referme avant tout traitement */
static void walk(fs::FS& fs, const String& dir, std::vector<Entry>& out, uint8_t depth = 0) {
  if (depth > 6) return;
  File d = fs.open(dir);
  if (!d || !d.isDirectory()) { if (d) d.close(); return; }
  std::vector<Entry> local;
  File f = d.openNextFile();
  while (f) {
    Entry e;
    e.path = f.path();
    if (!e.path.startsWith("/")) e.path = "/" + e.path;
    e.isDir = f.isDirectory();
    e.size = f.size();
    local.push_back(e);
    f.close();
    f = d.openNextFile();
  }
  d.close();
  for (const Entry& e : local) {
    if (e.isDir) walk(fs, e.path, out, depth + 1);
    else out.push_back(e);
  }
}

static void mkParents(fs::FS& fs, const String& path) {
  for (int i = 1; i < (int)path.length(); i++) {
    if (path[i] == '/') {
      String dir = path.substring(0, i);
      if (!fs.exists(dir)) fs.mkdir(dir);
    }
  }
}

static bool copyFile(fs::FS& sfs, const String& sp, fs::FS& dfs, const String& dp, bool dstIsSD, uint8_t* buf, size_t bs) {
  File in = sfs.open(sp, FILE_READ);
  if (!in) return false;
  if (dstIsSD) mkParents(dfs, dp);
  File out = dfs.open(dp, FILE_WRITE);
  if (!out) { in.close(); return false; }
  bool ok = true;
  const size_t total = in.size();
  size_t done = 0;
  while (done < total) {
    const size_t n = in.read(buf, bs);
    if (n == 0) { ok = false; break; }
    if (out.write(buf, n) != n) { ok = false; break; }
    done += n;
  }
  out.close();
  in.close();
  if (!ok) dfs.remove(dp);   // pas de fichier a moitie ecrit
  return ok;
}

bool SdBackup::start(bool restore) {
  if (_busy) return false;
  _busy = true;
  _ok = true;
  _restore = restore;
  _msg = restore ? "Restauration en cours..." : "Sauvegarde en cours...";
  if (xTaskCreate(_task, "sdbackup", 8192, this, 1, NULL) != pdPASS) {
    _busy = false;
    _ok = false;
    _msg = "Impossible de lancer la tache";
    return false;
  }
  return true;
}

void SdBackup::_task(void* arg) {
  ((SdBackup*)arg)->_run();
  vTaskDelete(NULL);
}

void SdBackup::_finish(bool ok, const String& msg) {
  _ok = ok;
  _msg = msg;
  FUNCTIONLOG("SdBackup", "%s", msg.c_str());
  _busy = false;
}

void SdBackup::_run() {
  bool mountedByUs = false;
  if (!sdman.ready) {                                   // en mode radio web la SD n'est pas montee
    if (!sdman.start()) { sdman.stop(); _finish(false, "Carte SD absente"); return; }
    mountedByUs = true;
  }
  #ifdef SLIDESHOW_ENABLE
    if (slideshow.active()) { slideshow.stop("sauvegarde"); vTaskDelay(pdMS_TO_TICKS(500)); }
  #endif
  if (config.getMode() == PM_SDCARD) { player.sendCommand({PR_STOP, 0}); vTaskDelay(pdMS_TO_TICKS(500)); }   // lecture SD arretee pendant la copie

  const size_t BS = 4096;
  uint8_t* buf = (uint8_t*)malloc(BS);
  if (!buf) { if (mountedByUs && config.getMode() == PM_WEB) sdman.stop(); _finish(false, "Memoire insuffisante"); return; }

  uint16_t files = 0, errors = 0;
  uint32_t bytes = 0;
  bool ok = true;
  String msg;

  if (!_restore) {
    // SPIFFS -> SD:/backup
    if (!sdman.exists(BACKUP_DIR)) sdman.mkdir(BACKUP_DIR);
    std::vector<Entry> list;
    walk(SPIFFS, "/", list);
    for (const Entry& e : list) {
      if (e.path == TMP_PATH) continue;                  // fichier temporaire
      if (copyFile(SPIFFS, e.path, sdman, String(BACKUP_DIR) + e.path, true, buf, BS)) { files++; bytes += e.size; }
      else { errors++; FUNCTIONLOG("SdBackup", "echec : %s", e.path.c_str()); }
    }
    ok = (errors == 0 && files > 0);
    msg = "Sauvegarde : " + String(files) + " fichier(s), " + String((bytes + 512) / 1024) + " Ko -> SD:" BACKUP_DIR;
    if (errors) msg += " (" + String(errors) + " erreur(s))";
    if (!files) msg = "Rien a sauvegarder";
  } else {
    // SD:/backup -> SPIFFS
    if (!sdman.exists(BACKUP_DIR)) {
      free(buf);
      if (mountedByUs && config.getMode() == PM_WEB) sdman.stop();
      _finish(false, "Aucune sauvegarde sur la SD (dossier " BACKUP_DIR ")");
      return;
    }
    std::vector<Entry> list;
    walk(sdman, BACKUP_DIR, list);
    const size_t prefix = strlen(BACKUP_DIR);
    for (const Entry& e : list) {
      if (!e.path.startsWith(BACKUP_DIR "/")) continue;
      String dst = e.path.substring(prefix);
      if (copyFile(sdman, e.path, SPIFFS, dst, false, buf, BS)) { files++; bytes += e.size; }
      else { errors++; FUNCTIONLOG("SdBackup", "echec : %s", dst.c_str()); }
    }
    ok = (errors == 0 && files > 0);
    msg = "Restauration : " + String(files) + " fichier(s), " + String((bytes + 512) / 1024) + " Ko depuis SD:" BACKUP_DIR;
    if (errors) msg += " (" + String(errors) + " erreur(s))";
    if (!files) msg = "Sauvegarde vide ou illisible";
    memo.reload();
  }
  free(buf);
  if (mountedByUs && config.getMode() == PM_WEB) sdman.stop();

  if (_restore && files > 0) {                           // la radio relit tout (playlist, wifi, pense-bete, pages web)
    _finish(ok, msg + " - redemarrage...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP.restart();
    return;
  }
  _finish(ok, msg);
}

String SdBackup::statusJson() {
  String m = _msg;
  m.replace("\\", "\\\\");
  m.replace("\"", "\\\"");
  return String("{\"busy\":") + (_busy ? "1" : "0") + ",\"ok\":" + (_ok ? "1" : "0") + ",\"msg\":\"" + m + "\"}";
}

#else   // carte sans lecteur SD : fonction inactive
bool SdBackup::start(bool) { _msg = "Pas de lecteur SD sur cette carte"; _ok = false; return false; }
String SdBackup::statusJson() { return "{\"busy\":0,\"ok\":0,\"msg\":\"Pas de lecteur SD sur cette carte\"}"; }
#endif

SdBackup sdbackup;
