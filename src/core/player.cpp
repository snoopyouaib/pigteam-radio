#include "options.h"
#include "player.h"
#include <algorithm>
#include <vector>
#include "audiohandlers.h"
#include "config.h"
#include "logging.h"
#include "telnet.h"
#include "display.h"
#include "sdmanager.h"
#include "backlightcontrols.h"
#include "netserver.h"
#include "network.h"
#include "rgbled.h"
#include "utility.h"
#include "../locale/dsplocale.h"

#ifdef USE_ES8311
  #include "../libraries/ES8311_Audio/es8311.h"
#endif

#define NETWORK_TASK_STACK_BYTES (NETWORK_TASK_STACK_SIZE * 1024)

Player player;
QueueHandle_t playerQueue;

#if defined(USE_AUDIO_VS1053)
  Player::Player(): Audio(VS1053_CS, VS1053_DCS, VS1053_DREQ, &VS1053_SPIBUS) {}
  void ResetChip() {
    pinMode(VS1053_RST, OUTPUT);
    digitalWrite(VS1053_RST, LOW);
    delay(30);
    digitalWrite(VS1053_RST, HIGH);
    delay(100);
  }
#else
  Player::Player() {}
#endif

void Player::init() {
  BOOTLOGX("player.init\t");
  playerQueue=NULL;
  //_resumeFilePos = 0;
  playerQueue = xQueueCreate(10, sizeof(playerRequestParams_t));
  setOutputPins(false);
  delay(50);
  memset(_plError, 0, PLERR_LN);
  memset(burl, 0, sizeof(burl));
  if (MUTE_PIN!=255) pinMode(MUTE_PIN, OUTPUT);
  #if defined(USE_AUDIO_I2S)
    setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_DIN, I2S_MCLK);
  #else
    // SPI.begin(); // started in config.init()
    if (VS1053_RST>0) ResetChip();
    begin(); // chip query + patch load now handled inside begin()
  #endif
  setAudioTaskCore(AUDIO_CORE);
  #if defined(USE_ES8311) && defined(ES8311_I2C_SDA) && defined(ES8311_I2C_SCL)
    // leaving this here but it seems not needed?  (this is the only operation that uses I2C on ES8311)
    if (es.begin(ES8311_I2C_SDA, ES8311_I2C_SCL, 400000UL)) es.setVolume(0); /* Start codec muted (or low) to avoid very loud output before saved volume is applied */
  #endif
  setBalance(config.store.balance);
  setTone(config.store.bass, config.store.middle, config.store.treble);
  setVolume(0);
  //_status = STOPPED;
  ////setOutputPins(false);
  //_volTimer=false;
  ////randomSeed(analogRead(0));
  #if PLAYER_FORCE_MONO
    forceMono(true);
  #endif
  _loadVol(config.store.volume);
  #ifdef USE_ES8311
    uint8_t i2sVol_init = volToI2S(config.store.volume); /* Also apply stored volume to codec (respecting station ovol via volToI2S) */
    es.setVolume((uint8_t)map(i2sVol_init, 0, VOLUME_SCALE, 0, 100)); /* Map I2S volume (0..VOLUME_SCALE) to codec volume 0..100 */
  #endif
  #ifdef CONNECT_HTTP_HTTPS_TIMEOUT // macro must be two numbers separated by a comma, ie: 1700, 3700
    setConnectionTimeout(CONNECT_HTTP_HTTPS_TIMEOUT);
  #endif
  if (rgbled.isInitialized()) rgbled.stopped();
  SERIALLOG("done");
}

void Player::sendCommand(playerRequestParams_t request) {
  if (playerQueue==NULL) return;
  if (xQueueSend(playerQueue, &request, pdMS_TO_TICKS(PLQ_SEND_DELAY)) != pdTRUE) {
    FUNCTIONLOG("Queue", "playerQueue overflow, dropped cmd=%d", request.type);
  }
}

void Player::resetQueue() {
	if (playerQueue!=NULL) xQueueReset(playerQueue);
}

void Player::stopSync() {
  _stop();  // synchronous stop — closes the audio file so SD can be unmounted safely
}

void Player::stopInfo() {
  //telnet.info();
  netserver.requestOnChange(MODE, 0);
}

void Player::setError(const char *e) {
  strlcpy(_plError, e, PLERR_LN);
  if (hasError()) {
    config.setTitle(_plError);
    ERRORLOG("%s", e);
  }
}

void Player::_stop(bool alreadyStopped) {
  log_i("%s called", __func__);
  #ifdef ALARM_ENABLE
    _alarmRamp = false;
    const bool wasAlarmFile = _alarmFileMode;
    _alarmFileMode = false;
    _alarmFileNext = false;
  #endif
  if (config.getMode()==PM_SDCARD && !alreadyStopped) config.sdResumePos = player.getFilePos();
  _status = STOPPED;
  _playingStationId = 0;
  setOutputPins(false);
  if (audioHandlers.clearArtwork()) netserver.requestOnChange(ARTWORK, 0);
  if (!hasError()) config.setTitle((display.mode()==LOST || display.mode()==UPDATING)?"":l10n(L10N_MSG_STOPPED));
  config.station.bitrate = 0;
  config.setBitrateFormat(BF_UNKNOWN);
  netserver.requestOnChange(BITRATE, 0);
  display.putRequest(DBITRATE);
  display.putRequest(PSTOP);
  setDefaults();
  if (!alreadyStopped) stopSong();
  if (!lockOutput) stopInfo();
  rgbled.stopped();
  backlightControls.restart();
  #if defined(ALARM_ENABLE) && defined(USE_SD)
    if (wasAlarmFile && config.getMode()==PM_WEB) sdman.stop();   // la SD n'est pas montee en mode radio web
  #endif
}

void Player::initHeaders(const char *file) {
  if (strlen(file)==0 || true) return; //TODO Read TAGs (SD Mode) (may never be implemented because I2S already handles metadata correctly, the issue only exists on VS1053 non-MP3)
  #ifdef USE_SD
    connecttoFS(sdman,file);
    eofHeader = false;
    while(!eofHeader) Audio::loop();
    //netserver.requestOnChange(SDPOS, 0);
  #endif
  setDefaults();
}

#ifndef PL_QUEUE_TICKS
  #define PL_QUEUE_TICKS 0
#endif
#ifndef PL_QUEUE_TICKS_ST
  #define PL_QUEUE_TICKS_ST 15
#endif
void Player::loop() {
  if (playerQueue==NULL) return;
  playerRequestParams_t requestP;
  if (xQueueReceive(playerQueue, &requestP, isRunning()?PL_QUEUE_TICKS:PL_QUEUE_TICKS_ST)) {
    switch (requestP.type) {
      case PR_STOP: _stop(); break;
      case PR_PLAY: {
        if (requestP.payload>0) {
          config.setLastStation((uint16_t)requestP.payload);
        }
        _play((uint16_t)abs(requestP.payload)); 
        break;
      }
      case PR_TOGGLE: {
        toggle();
        break;
      }
      case PR_VOL: {
        #ifdef ALARM_ENABLE
          _alarmRamp = false; // un reglage de volume manuel interrompt la montee progressive du reveil
        #endif
        config.setVolume(requestP.payload);
        config.saveValueButWait(&config.store.volume, (uint8_t)requestP.payload, 3000);
        uint8_t i2sVol = volToI2S(requestP.payload);
        Audio::setVolume(i2sVol);
        #ifdef USE_ES8311
          es.setVolume((uint8_t)map(i2sVol, 0, VOLUME_SCALE, 0, 100)); /* Map I2S volume (already adjusted for station ovol) 0..VOLUME_SCALE -> codec 0..100 */
        #endif
        break;
      }
      #ifdef USE_SD
        case PR_CHECKSD: {
          if (config.getMode()==PM_SDCARD) {
            if (!sdman.cardPresent()) {
              sdman.stop();
              config.changeMode(PM_WEB);
            }
          }
          break;
        }
      #endif
      case PR_VUTONUS: {
        if (config.vuThreshold>10) config.vuThreshold -=10;
        break;
      }
      case PR_BURL: {
        if (strlen(burl) > 0) browseUrl();
        break;
      }
      default: break;
    }
  }
  Audio::loop();
  if (!isRunning() && _status==PLAYING && !_alarmFileMode) {
    // Stream died unexpectedly - trigger reconnection if WiFi is still up
    if (config.getMode() == PM_WEB && WiFi.status() == WL_CONNECTED && !network.lostPlaying) {
      FUNCTIONLOG("Player", "Stream stopped unexpectedly. Starting reconnection attempts...");
      network.lostPlaying = true;
      // Launch retry task if not already running
      if (streamRetryTaskHandle == NULL) {
        xTaskCreatePinnedToCore(retryStreamConnection, "streamRetry", NETWORK_TASK_STACK_BYTES, NULL, NET_TASK_PRIORITY, &streamRetryTaskHandle, NETWORK_CORE);
      }
    }
    _stop(true);
  }
  if (strlen(burl) > 0) browseUrl();
  #ifdef SLEEP_TIMER_ENABLE
    _sleepTick();
  #endif
  #ifdef ALARM_ENABLE
    _alarmTick();
  #endif
}

bool Player::queueResolvedUrl(const char* url) {
  if (url == nullptr || url[0] == '\0') return false;
  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) return false;

  uint16_t foundIdx = utility.findStationByUrl(url);
  if (foundIdx > 0) {
    sendCommand({PR_PLAY, foundIdx});
    return true;
  }

  if (strlen(url) >= sizeof(burl)) return false;
  strlcpy(burl, url, sizeof(burl));
  sendCommand({PR_BURL, 0});
  return true;
}

bool Player::resumeLastWebSource() {
  if (config.store.lastStationUrl[0] != '\0') {
    uint16_t foundIdx = utility.findStationByUrl(config.store.lastStationUrl);
    if (foundIdx > 0) {
      sendCommand({PR_PLAY, foundIdx});
      return true;
    }
    return queueResolvedUrl(config.store.lastStationUrl);
  }

  uint16_t stationId = config.lastStation();
  if (stationId == 0) return false;

  sendCommand({PR_PLAY, stationId});
  return true;
}

void Player::setOutputPins(bool isPlaying) {
  if (LED_PIN!=255) digitalWrite(LED_PIN, LED_INVERT?!isPlaying:isPlaying);
  bool _ml = MUTE_LOCK?!MUTE_VAL:(isPlaying?!MUTE_VAL:MUTE_VAL);
  if (MUTE_PIN!=255) digitalWrite(MUTE_PIN, _ml);
}

void Player::_play(uint16_t stationId) {
  log_i("%s called, stationId=%d", __func__, stationId);
  if (_status == PLAYING && stationId > 0 && stationId == _playingStationId) return;
  #ifdef ALARM_ENABLE
    _alarmFileMode = false;   // une autre lecture prend le relais du MP3 de reveil
    _alarmFileNext = false;
  #endif
  setError("");
  setDefaults();
  remoteStationName = false;
  if (audioHandlers.clearArtwork()) netserver.requestOnChange(ARTWORK, 0);
  config.setDspOn(1);
  config.vuThreshold = 0;
  //display.putRequest(PSTOP);
  config.screensaverTicks=SCREENSAVERSTARTUPDELAY;
  config.screensaverPlayingTicks=SCREENSAVERSTARTUPDELAY;
  if (config.getMode()!=PM_SDCARD) {
    display.putRequest(PSTOP);
  }
  setOutputPins(false);
  //config.setTitle(config.getMode()==PM_WEB?const_PlConnect:"");
  if (!utility.loadStation(stationId)) return;
  config.setTitle(config.getMode()==PM_WEB?l10n(L10N_MSG_CONNECT):"[next track]");
  config.station.bitrate=0;
  config.setBitrateFormat(BF_UNKNOWN);
  
  _loadVol(config.store.volume);
  display.putRequest(DBITRATE);
  display.putRequest(NEWSTATION);
  netserver.requestOnChange(STATION, 0);
  bool isConnected = false;
  if (config.getMode()==PM_SDCARD && SD_CS!=255) {
    uint32_t _t_cfs = millis();
    isConnected=connecttoFS(sdman,config.station.url,config.sdResumePos==0?_resumeFilePos:config.sdResumePos-player.sd_min);
    FUNCTIONLOG("SD", "connecttoFS: %lums", millis() - _t_cfs);
  } else {
    config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB));
  }
  if (config.getMode()==PM_WEB) {
    uint32_t _t_cth = millis();
    isConnected=connecttohost(config.station.url);
    FUNCTIONLOG("SD", "connecttohost: %lums", millis() - _t_cth);
    // Note: Removed blind retry - if connection fails (timeout/404/refused), retrying 1.5s later
    // won't help and just adds 20+ seconds of delay. Stream reconnection is now handled by the
    // retryStreamConnection task which monitors for unexpected disconnects during playback.
  }
  if (isConnected) {
  //if (config.store.play_mode==PM_WEB?connecttohost(config.station.url):connecttoFS(SD,config.station.url,config.sdResumePos==0?_resumeFilePos:config.sdResumePos-player.sd_min)) {
    _status = PLAYING;
    _playingStationId = stationId;
    if (config.getMode()==PM_SDCARD) {
      config.sdResumePos = 0;
      config.saveValue(&config.store.lastSdStation, stationId);
    } else {
      config.saveLastStationUrl(config.station.url);
    }
    //config.setTitle("");
    netserver.requestOnChange(MODE, 0);
    setOutputPins(true);
    display.putRequest(NEWMODE, PLAYER);
    display.putRequest(PSTART);
    #ifdef RADIO_BROWSER_SEND_CLICKS
      if (config.getMode()==PM_WEB && !network.lostPlaying) radioBrowserSendClick(config.station.url);
    #endif
    network.lostPlaying = false;  // Clear flag - we're playing again!
    rgbled.playing();
    backlightControls.restart();
  } else {
    // Self-healing: if SD file vanished, force re-index so next/prev uses fresh index
    #ifdef USE_SD
      if (config.getMode()==PM_SDCARD && !sdman.exists(config.station.url)) {
        FUNCTIONLOG("SD", "File not found. Re-indexing.");
        display.putRequest(PSTOP);                   // clear playback screen
        display.putRequest(NEWMODE, SDCHANGE);       // show on-screen counter
        config.initSDPlaylist(true);
        display.putRequest(NEWMODE, PLAYER);         // restore player mode
        display.putRequest(NEWSTATION);
      }
    #endif
    ERRORLOG("Error connecting to %s", config.station.url);
    char errbuf[STATION_FIELD_LENGTH];
    snprintf_P(errbuf, sizeof(errbuf), l10n(L10N_MSG_CONNECT_ERROR), config.station.url);
    player.setError(errbuf);
    _stop(true);
  };
}

void Player::browseUrl() {
  if (burl[0] == '\0') return;
  playUrl(burl);
  memset(burl, 0, sizeof(burl));
}

void Player::playUrl(const char* url) {
  setError("");
  remoteStationName = true;
  if (audioHandlers.clearArtwork()) netserver.requestOnChange(ARTWORK, 0);
  config.setDspOn(1);
  resumeAfterUrl = _status==PLAYING;
  display.putRequest(PSTOP);
  setOutputPins(false);
  config.setTitle(l10n(L10N_MSG_CONNECT));
  if (connecttohost(url)) {
    _status = PLAYING;
    config.saveLastStationUrl(url);
    config.setTitle("");
    netserver.requestOnChange(MODE, 0);
    setOutputPins(true);
    display.putRequest(PSTART);
    #ifdef RADIO_BROWSER_SEND_CLICKS
      if (!network.lostPlaying) radioBrowserSendClick(url);
    #endif
    rgbled.playing();
    backlightControls.restart();
  } else {
    ERRORLOG("Error connecting to %s", url);
    SET_PLAY_ERROR("Error connecting to %s", url);
    _stop(true);
  }
}

void Player::prev() {
  if (streamRetryTaskHandle != NULL) {
    network.lostPlaying = false;
    vTaskDelete(streamRetryTaskHandle);
    streamRetryTaskHandle = NULL;
  }
  uint16_t lastStation = config.lastStation();
  if (config.getMode()==PM_WEB || !config.store.sdshuffle) {
    if (lastStation == 1) config.lastStation(utility.playlistLength()); else config.lastStation(lastStation-1);
  }
  sendCommand({PR_PLAY, config.lastStation()});
}

void Player::next() {
  if (streamRetryTaskHandle != NULL) {
    network.lostPlaying = false;
    vTaskDelete(streamRetryTaskHandle);
    streamRetryTaskHandle = NULL;
  }
  uint16_t lastStation = config.lastStation();
  if (config.getMode()==PM_WEB || !config.store.sdshuffle) {
    if (lastStation == utility.playlistLength()) config.lastStation(1); else config.lastStation(lastStation+1);
  } else {
    config.lastStation(random(1, utility.playlistLength()));
  }
  sendCommand({PR_PLAY, config.lastStation()});
}

void Player::toggle() {
  if (streamRetryTaskHandle != NULL) {
    network.lostPlaying = false;
    vTaskDelete(streamRetryTaskHandle);
    streamRetryTaskHandle = NULL;
  }
  if (_status == PLAYING) {
    sendCommand({PR_STOP, 0});
  } else {
    if (config.getMode() == PM_WEB) {
      resumeLastWebSource();
    } else {
      sendCommand({PR_PLAY, config.lastStation()});
    }
  }
}

void Player::stepVol(bool up) {
  if (up) {
    if (config.store.volume < VOLUME_SCALE) {
      setVol(config.store.volume + 1);
    } else {
      setVol(VOLUME_SCALE);
    }
  } else {
    if (config.store.volume > 0) {
      setVol(config.store.volume - 1);
    } else {
      setVol(0);
    }
  }
}

uint8_t Player::volToI2S(uint8_t volume) {
  #ifdef USE_ES8311
    // Apply gamma curve for ES3C28P to make low volumes more audible
    int maxIn = VOLUME_SCALE - config.station.ovol * 3;
    if (maxIn < 1) maxIn = 1; // avoid division by zero; treat invalid ovol as no reduction
    if (volume > (uint8_t)maxIn) volume = (uint8_t)maxIn;
    float vnorm = (float)volume / (float)maxIn; // 0..1
    if (vnorm < 0.0f) vnorm = 0.0f;
    if (vnorm > 1.0f) vnorm = 1.0f;
    /* Apply gamma curve (sqrt) to make low volumes more audible and top end less aggressive */
    const float gamma = 0.5f;
    float vout = powf(vnorm, gamma);
    int vol = (int)(vout * (float)VOLUME_SCALE + 0.5f);
    if (vol > VOLUME_SCALE) vol = VOLUME_SCALE;
    if (vol < 0) vol = 0;
    return (uint8_t)vol;
  #else
    int vol = map(volume, 0, VOLUME_SCALE - config.station.ovol * 3 , 0, VOLUME_SCALE);
    if (vol > VOLUME_SCALE) vol = VOLUME_SCALE;
    if (vol < 0) vol = 0;
    return vol;
  #endif
}

void Player::_loadVol(uint8_t volume) {
  #ifdef ALARM_ENABLE
    if (_alarmRamp) return; // montee progressive du reveil en cours : le volume est pilote par _alarmTick
  #endif
  setVolume(volToI2S(volume));
}

void Player::setVol(uint8_t volume) {
  player.sendCommand({PR_VOL, volume});
}

#if defined(SLEEP_TIMER_ENABLE) || defined(ALARM_ENABLE)
void Player::_applyI2SVolume(uint8_t i2sVol) {
  Audio::setVolume(i2sVol);
  #ifdef USE_ES8311
    es.setVolume((uint8_t)map(i2sVol, 0, VOLUME_SCALE, 0, 100));
  #endif
}
#endif

#ifdef SLEEP_TIMER_ENABLE
void Player::startSleepTimer(uint16_t minutes) {
  if (minutes == 0) { cancelSleepTimer(); return; }
  _sleepMinutes = minutes;
  _sleepEndMs = millis() + (uint32_t)minutes * 60000UL;
  _sleepLastVol = -1;
  _sleepActive = true;
  FUNCTIONLOG("SleepTimer", "Arret programme dans %u min", minutes);
  _sleepLastMin = minutes;
  display.putRequest(DSPSLEEP);   // "(arret N min)" ajoute au titre affiche, titre de la radio conserve
  netserver.requestOnChange(SLEEPTIMER, 0);
}

void Player::cancelSleepTimer() {
  bool wasActive = _sleepActive;
  _sleepActive = false;
  _sleepMinutes = 0;
  if (wasActive) {
    if (_sleepLastVol >= 0) _applyI2SVolume(volToI2S(config.store.volume)); // fondu en cours : on rend le volume
    _sleepLastVol = -1;
    FUNCTIONLOG("SleepTimer", "Arret programme annule");
    display.putRequest(DSPSLEEP);
    netserver.requestOnChange(SLEEPTIMER, 0);
  }
}

uint32_t Player::sleepRemaining() {
  if (!_sleepActive) return 0;
  int32_t left = (int32_t)(_sleepEndMs - millis());
  return left > 0 ? (uint32_t)(left / 1000) : 0;
}

void Player::_sleepTick() {
  if (!_sleepActive) return;
  int32_t left = (int32_t)(_sleepEndMs - millis()); // ms, valable meme apres debordement de millis()
  if (left <= 0) {
    _sleepActive = false;
    _sleepMinutes = 0;
    _sleepLastVol = -1;
    FUNCTIONLOG("SleepTimer", "Fin du compte a rebours : arret");
    if (_status == PLAYING || isRunning()) _stop();
    _applyI2SVolume(volToI2S(config.store.volume)); // volume normal pour la prochaine lecture
    display.putRequest(DSPSLEEP);
    netserver.requestOnChange(SLEEPTIMER, 0);
    return;
  }
  uint32_t minsLeft = ((uint32_t)left + 59999UL) / 60000UL;   // minutes restantes, arrondies au-dessus
  if (minsLeft != _sleepLastMin) { _sleepLastMin = minsLeft; display.putRequest(DSPSLEEP); }
  if ((uint32_t)left < SLEEP_FADE_MS && isRunning()) {
    uint8_t base = volToI2S(config.store.volume);
    uint8_t v = (uint8_t)(((uint32_t)base * (uint32_t)left) / SLEEP_FADE_MS);
    if (v != _sleepLastVol) { _sleepLastVol = v; _applyI2SVolume(v); }
  }
}
#endif // SLEEP_TIMER_ENABLE

#ifdef ALARM_ENABLE
void Player::_alarmFire() {
  FUNCTIONLOG("Alarm", "Reveil declenche");
  #ifdef SLEEP_TIMER_ENABLE
    cancelSleepTimer();
  #endif
  // on rallume l'ecran et on sort de l'economiseur
  config.setDspOn(true);
  backlightControls.restart();
  config.screensaverTicks = 0;
  config.screensaverPlayingTicks = 0;
  display.putRequest(NEWMODE, PLAYER);
  if (_status == PLAYING || isRunning()) return; // deja en lecture : on ne touche a rien
  if (config.getMode() != PM_WEB) config.changeMode(PM_WEB);
  FUNCTIONLOG("Alarm", "Montee du volume : station %u, volume final %u", (unsigned)config.store.alarmstation, (unsigned)config.store.alarmvol);
  _alarmRamp = true;
  _alarmRampStart = millis();
  _alarmLastCheck = 0;
  _applyI2SVolume(0);
  #ifdef USE_SD
    if (config.store.alarmfile) {
      if (_alarmPlayFile()) return;
      FUNCTIONLOG("Alarm", "Aucun fichier audio utilisable dans /reveil : on joue la station");
    }
  #endif
  uint16_t id = config.store.alarmstation;
  if (id > utility.playlistLength()) id = 0; // station disparue de la liste : derniere station
  if (id > 0) {
    sendCommand({PR_PLAY, id});
  } else if (!resumeLastWebSource()) {
    sendCommand({PR_PLAY, config.lastStation()});
  }
}

#ifdef USE_SD
/* Tirage au sort uniforme (sans tout garder en memoire) d'un fichier audio dans un dossier, sous-dossiers compris */
static void alarmScanDir(const char* dirname, uint8_t levels, String& pick, uint32_t& n) {
  File dir = sdman.open(dirname);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return; }
  for (;;) {
    vTaskDelay(1);
    bool isDir = false;
    String f = dir.getNextFileName(&isDir);
    if (f.isEmpty()) break;
    String l = f;
    l.toLowerCase();
    const char* b = strrchr(l.c_str(), '/');
    b = b ? b + 1 : l.c_str();
    if (b[0] == '.') continue;                     // fichiers caches / ._ de macOS
    if (isDir) {
      if (levels && strcmp(b, "system volume information") != 0) alarmScanDir(f.c_str(), levels - 1, pick, n);
      continue;
    }
    if (!(l.endsWith(".mp3") || l.endsWith(".m4a") || l.endsWith(".aac") || l.endsWith(".wav") || l.endsWith(".flac") || l.endsWith(".ogg") || l.endsWith(".opus"))) continue;
    n++;
    if (esp_random() % n == 0) pick = f;
  }
  dir.close();
}

static bool alarmListDir(const char* dirname, uint8_t levels, std::vector<String>& out, const char* prefix) {
  File dir = sdman.open(dirname);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return false; }
  for (;;) {
    vTaskDelay(1);
    bool isDir = false;
    String f = dir.getNextFileName(&isDir);
    if (f.isEmpty()) break;
    String l = f;
    l.toLowerCase();
    const char* b = strrchr(l.c_str(), '/');
    b = b ? b + 1 : l.c_str();
    if (b[0] == '.') continue;
    if (isDir) { if (levels && out.size() < 200) alarmListDir(f.c_str(), levels - 1, out, prefix); continue; }
    if (!(l.endsWith(".mp3") || l.endsWith(".m4a") || l.endsWith(".aac") || l.endsWith(".wav") || l.endsWith(".flac") || l.endsWith(".ogg") || l.endsWith(".opus"))) continue;
    if (out.size() >= 200) break;
    String rel = f;
    if (rel.startsWith(prefix)) rel = rel.substring(strlen(prefix));
    out.push_back(rel);
  }
  dir.close();
  return true;
}

/* Page Outils, bouton « Scanner /reveil » : liste des fichiers audio du dossier (JSON). La SD est montee le temps du scan. */
String Player::alarmScanJson() {
  if (SD_CS == 255) return "{\"err\":\"Pas de carte SD configurée\"}";
  if (_alarmFileMode || (config.getMode() == PM_SDCARD && isRunning())) return "{\"err\":\"Lecture en cours : arrête-la avant de scanner\"}";
  const bool wasReady = sdman.ready;
  if (!wasReady && !sdman.start()) { sdman.stop(); return "{\"err\":\"Carte SD absente ou illisible\"}"; }
  std::vector<String> files;
  const bool found = alarmListDir("/reveil", 2, files, "/reveil/");
  if (!wasReady && config.getMode() == PM_WEB) sdman.stop();
  if (!found) return "{\"err\":\"Dossier /reveil introuvable à la racine de la carte\"}";
  std::sort(files.begin(), files.end(), [](const String& a, const String& b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });
  String o = "{\"count\":" + String((unsigned)files.size()) + ",\"files\":[";
  for (size_t i = 0; i < files.size(); i++) {
    String n = files[i];
    n.replace("\\", "\\\\");
    n.replace("\"", "\\\"");
    if (i) o += ',';
    o += '"'; o += n; o += '"';
  }
  o += "]}";
  FUNCTIONLOG("Alarm", "Scan /reveil : %u fichiers", (unsigned)files.size());
  return o;
}

/* Reveil par MP3 : tire un fichier audio dans /reveil de la carte SD (le lecteur SD n'indexe pas ce dossier,
   voir sdmanager.cpp). La radio reste en mode web. */
bool Player::_alarmPlayFile() {
  if (SD_CS == 255) return false;
  if (!sdman.ready && !sdman.start()) { sdman.stop(); return false; }
  String pick;
  uint32_t n = 0;
  bool chosen = false;
  if (config.store.alarmtrack[0]) {                       // fichier choisi depuis la page Outils
    String t = String("/reveil/") + config.store.alarmtrack;
    if (sdman.exists(t.c_str())) { pick = t; n = 1; chosen = true; }
    else FUNCTIONLOG("Alarm", "Fichier choisi introuvable (%s) : tirage au hasard", config.store.alarmtrack);
  }
  if (!chosen) alarmScanDir("/reveil", 2, pick, n);
  if (pick.isEmpty()) { if (config.getMode() == PM_WEB) sdman.stop(); return false; }
  if (pick[0] != '/') pick = String("/") + pick;
  FUNCTIONLOG("Alarm", "MP3 : %s (%u fichiers)", pick.c_str(), (unsigned)n);
  if (!connecttoFS(sdman, pick.c_str())) {
    FUNCTIONLOG("Alarm", "Lecture impossible : %s", pick.c_str());
    if (config.getMode() == PM_WEB) sdman.stop();
    return false;
  }
  const char* base = strrchr(pick.c_str(), '/');
  base = base ? base + 1 : pick.c_str();
  _alarmFileMode = true;
  _alarmFileNext = false;
  _status = PLAYING;
  config.setStation("Reveil");
  config.setTitle(base);
  display.putRequest(NEWSTATION);
  netserver.requestOnChange(STATION, 0);
  setOutputPins(true);
  netserver.requestOnChange(MODE, 0);
  display.putRequest(NEWMODE, PLAYER);
  display.putRequest(PSTART);
  rgbled.playing();
  backlightControls.restart();
  return true;
}
#endif

void Player::_alarmTick() {
  uint32_t now = millis();
  #ifdef USE_SD
    if (_alarmFileMode && _alarmFileNext) {   // fin d'un MP3 : on en tire un autre tant que le reveil n'est pas arrete
      _alarmFileNext = false;
      _alarmFileMode = false;
      if (!_alarmPlayFile()) _stop(true);
    }
  #endif
  if (_alarmRamp) {
    if (now - _alarmLastCheck < 100) return;
    _alarmLastCheck = now;
    uint32_t el = now - _alarmRampStart;
    if (el >= ALARM_RAMP_MS) {
      _alarmRamp = false;
      FUNCTIONLOG("Alarm", "Fin de la montee : volume %u (i2s %u)", (unsigned)config.store.alarmvol, (unsigned)volToI2S(config.store.alarmvol));
      _applyI2SVolume(volToI2S(config.store.alarmvol)); // applique tout de suite
      setVol(config.store.alarmvol); // puis passe par PR_VOL (enregistre et met l'interface a jour)
    } else {
      uint8_t target = volToI2S(config.store.alarmvol);
      uint8_t v = (uint8_t)(((uint32_t)target * el) / ALARM_RAMP_MS);
      static uint32_t lastLog = 0;
      if (now - lastLog >= 10000) { lastLog = now; FUNCTIONLOG("Alarm", "Montee : i2s %u / %u", (unsigned)v, (unsigned)target); }
      _applyI2SVolume(v);
    }
    return;
  }
  if (now - _alarmLastCheck < 1000) return;
  _alarmLastCheck = now;
  if (!config.store.alarmon) return;
  const struct tm &t = network.timeinfo;
  if (t.tm_year <= 100) return; // heure pas encore synchronisee
  if (t.tm_hour != config.store.alarmhour || t.tm_min != config.store.alarmmin) return;
  if (!((config.store.alarmdays >> t.tm_wday) & 1)) return;
  uint32_t key = (uint32_t)t.tm_yday * 1440UL + (uint32_t)t.tm_hour * 60UL + (uint32_t)t.tm_min + 1UL;
  if (key == _alarmLastKey) return; // deja declenche pendant cette minute
  _alarmLastKey = key;
  _alarmFire();
}
#endif // ALARM_ENABLE
