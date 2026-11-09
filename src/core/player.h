#ifndef player_h
#define player_h

#include "options.h"

#if defined(USE_AUDIO_I2S)
 #include "../libraries/I2S_Audio/Audio.h"
#else
  #include "../libraries/VS1053_Audio/audioVS1053Ex.h"
#endif

#define PLERR_LN        64
#define SET_PLAY_ERROR(...) {char buff[576]; snprintf(buff, sizeof(buff), __VA_ARGS__); setError(buff);}

enum playerRequestType_e : uint8_t { PR_PLAY = 1, PR_STOP = 2, PR_PREV = 3, PR_NEXT = 4, PR_VOL = 5, PR_CHECKSD = 6, PR_VUTONUS = 7, PR_BURL = 8, PR_TOGGLE = 9 };
struct playerRequestParams_t
{
  playerRequestType_e type;
  int payload;
};

enum plStatus_e : uint8_t{ PLAYING = 1, STOPPED = 2 };

/* Arret programme (sleep timer) : volume en fondu puis arret. Exclu de l'env Zero (flash presque plein). */
#ifndef CARTE_ZERO
  #define SLEEP_TIMER_ENABLE
  #define SLEEP_FADE_MS 120000UL   // duree du fondu avant l'arret
  #define ALARM_ENABLE            // reveil (heure, jours, station, volume progressif)
  #define ALARM_RAMP_MS 30000UL   // duree de la montee progressive du volume
#endif

class Player: public Audio {
  public:
    volatile bool lockOutput = true;  // volatile: written from WiFi callbacks, read from audio callbacks
    bool resumeAfterUrl = false;
    uint32_t sd_min, sd_max;
    bool remoteStationName = false;
    char burl[MQTT_URL_SIZE + 1];  /* buffer for browseUrl  */
  public:
    Player();
    void init();
    void sendCommand(playerRequestParams_t request);
    void resetQueue();
    void stopSync();  // synchronous stop — closes audio file before SD unmount
    void stopInfo();
    void setError(const char *e);
    void initHeaders(const char *file);
    void loop();
    bool queueResolvedUrl(const char* url);
    bool resumeLastWebSource();
    void setOutputPins(bool isPlaying);
    void browseUrl();
    void playUrl(const char* url);
    void prev();
    void next();
    void toggle();
    void stepVol(bool up);
    uint8_t volToI2S(uint8_t volume);
    void setVol(uint8_t volume);
    #if defined(SLEEP_TIMER_ENABLE) || defined(ALARM_ENABLE)
      void _applyI2SVolume(uint8_t i2sVol);
    #endif
    #ifdef SLEEP_TIMER_ENABLE
      void startSleepTimer(uint16_t minutes);   // 0 = annule
      void cancelSleepTimer();
      uint16_t sleepMinutes() { return _sleepActive ? _sleepMinutes : 0; }
      uint32_t sleepRemaining();                // secondes restantes
    #endif

    bool hasError() { return strlen(_plError)>0; }
    plStatus_e status() { return _status; }
    void setResumeFilePos(uint32_t pos) { _resumeFilePos = pos; }
  private:
    uint32_t    _resumeFilePos = 0;
    plStatus_e  _status = STOPPED;
    uint16_t    _playingStationId = 0;
    char        _plError[PLERR_LN];

    void _stop(bool alreadyStopped = false);
    void _play(uint16_t stationId);
    void _loadVol(uint8_t volume);
    #ifdef SLEEP_TIMER_ENABLE
      bool     _sleepActive = false;
      uint16_t _sleepMinutes = 0;
      uint32_t _sleepEndMs = 0;
      int16_t  _sleepLastVol = -1;
      void _sleepTick();
    #endif
    #ifdef ALARM_ENABLE
      bool     _alarmRamp = false;
      uint32_t _alarmRampStart = 0;
      uint32_t _alarmLastKey = 0;
      uint32_t _alarmLastCheck = 0;
      void _alarmTick();
      void _alarmFire();
    #endif
};

extern Player player;

#endif
