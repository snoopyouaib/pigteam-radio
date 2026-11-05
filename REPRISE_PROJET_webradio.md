# 🔄 REPRISE PROJET — Web-radio ehRadio (ESP32-S3 N16R8)

> Document de reprise rapide. En cas de nouvelle session, lire ce fichier en premier.
> Dernière mise à jour : 5 novembre 2026.

---

## 📦 État actuel

Web-radio basée sur **ehRadio** (trip5, v2026.08.19, fork de yoRadio) sur **ESP32-S3 N16R8** (16 Mo flash, 8 Mo PSRAM octale), compilée avec PlatformIO (VS Code, Windows).
Dossier : `D:\vscode_projets\PigTeamConcept2026\webradio` — environnement PlatformIO `ehradio_s3_n16r8`. Ports série : COM14 (PC maison), COM6 (autre PC).

| Élément | État |
|---|---|
| Compilation, flash, démarrage, PSRAM 8 Mo | ✅ validé |
| Wi-Fi + WebUI (`settings.html` sans plantage) | ✅ validé (après correctif `netserver.cpp`) |
| Fuseau Europe/Paris, langue fr_FR | ✅ validé |
| Écran ILI9341 240x320 SPI | ✅ validé |
| Tactile XPT2046 (glissement = volume) | ✅ validé |
| Connexion aux flux (4 Radio France + SomaFM) | ✅ le log montre la connexion, **sans son** |
| **Sortie son PCM5102** | ⏳ **pas encore testée** (module attendu) |
| Luminosité réglable de l'écran | ❌ `BRIGHTNESS_PIN` non défini (LED branchée sur 3,3 V) |
| Sauvegarde Git de `webradio/` | ❌ à faire (voir plus bas) |

---

## 🔌 Câblage

**Écran ILI9341 + tactile XPT2046** (bus SPI A = broches par défaut de l'S3) :

| Module écran | ESP32-S3 |
|---|---|
| SCK + T_CLK | GPIO 12 |
| SDI (MOSI) + T_DIN | GPIO 11 |
| SDO (MISO) + T_DO | GPIO 13 |
| CS | GPIO 10 |
| DC | GPIO 9 |
| RESET | GPIO 8 |
| T_CS | GPIO 14 |
| LED | 3,3 V |
| VCC / GND | 3,3 V / GND |
| T_IRQ | non branché |

**DAC PCM5102 (I2S)** :

| PCM5102 | ESP32-S3 |
|---|---|
| DIN | GPIO 16 |
| BCK | GPIO 17 |
| LCK | GPIO 15 |
| VIN / GND | 3,3 V / GND |
| Ponts au dos : SCK, FLT, DMP, FMT | GND |
| Pont au dos : XMT | 3,3 V |

⚠️ N16R8 : **GPIO 26 à 37 interdits** (flash et PSRAM octale).
Le PCM5102 sort un niveau ligne : il faut une enceinte amplifiée, un ampli ou un casque.

---

## 📋 Fichiers de configuration

`myoptions.h` (à la racine, à côté de `platformio.ini`) :

```c
#ifndef myoptions_h
#define myoptions_h

#define DSP_MODEL     DSP_ILI9341
#define SPIA_DEFAULT                  /* SCK=12, MISO=13, MOSI=11 */

#define TFT_CS        10
#define TFT_DC         9
#define TFT_RST        8

#define TS_MODEL      TS_MODEL_XPT2046
#define TS_SPI        'A'
#define TS_CS         14

#define I2S_DOUT      16
#define I2S_BCLK      17
#define I2S_LRC       15
#define VS1053_CS     255

#endif
```

`platformio.ini` — environnement (les sections `[ehradio]` et `[library]` sont celles du dépôt ehRadio) :

```ini
[env:ehradio_s3_n16r8]
extends = ehradio
board = esp32-s3-devkitc-1
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
monitor_filters = esp32_exception_decoder, default
build_src_filter =
  ${ehradio.build_src_filter}
  +<libraries/I2S_Audio/*>
build_flags =
  ${ehradio.build_flags}
  -DBOARD_HAS_PSRAM
lib_deps =
  ${ehradio.lib_deps}
  ${library.ili9341}
  https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
```

---

## 🛠️ Correctifs et pièges rencontrés

1. **Plantage de `/settings.html` sans écran** (Guru Meditation, LoadProhibited, `netserver.cpp:537`) : `vuConf_ptr`, `bufferbarConf_ptr` et `weatherConf_ptr` valent `NULL` quand `DSP_MODEL = DSP_DUMMY`. Correctif dans `src/core/netserver.cpp` (lignes 537, 539, 554) : ajouter un test de pointeur nul, ex. `(vuConf_ptr && vuConf_ptr->textsize > 0) || DBGWUI`. À signaler à trip5/ehRadio. Avec un écran configuré, le plantage n'a pas lieu, mais le correctif reste utile.
2. **Erreur de lien `undefined reference to Audio::...`** : il manquait `+<libraries/I2S_Audio/*>` dans `build_src_filter` (ehRadio exclut tout par défaut).
3. **Erreur `XPT2046_Touchscreen::begin(SPIClass&)`** : la version du registre PlatformIO est trop ancienne ; utiliser la version GitHub (voir `lib_deps`). Après le changement : supprimer `.pio\libdeps\ehradio_s3_n16r8\XPT2046_Touchscreen`.
4. **Sans écran, `settings.html` cache beaucoup d'options** (affichage, météo, thèmes, économiseur…). Le sélecteur de fuseau était alors introuvable. Contournement par la console du navigateur (F12) sur `settings.html` :
   ```js
   websocket.send("tz_name=Europe/Paris");
   websocket.send("tzposix=CET-1CEST,M3.5.0,M10.5.0/3");
   ```
5. **Fuseau par défaut `Canada/Atlantic`** : les coordonnées météo par défaut sont celles de Halifax. Mettre les siennes (le champ français veut la virgule : ex. `49,9229` / `1,0775` pour Dieppe).
6. **SAFE MODE** au démarrage : normal après un reflash ; il disparaît après 30 s de démarrage stable (`Boot stable after 30000 ms`).

---

## 🚀 Procédure pour un ESP32 neuf

1. `pio run -e ehradio_s3_n16r8 -t uploadfs` (pages web, **d'abord**).
2. `pio run -e ehradio_s3_n16r8 -t upload` (firmware).
3. Se connecter au point d'accès `ehRadio` (sans mot de passe), `http://192.168.4.1/`, saisir le Wi-Fi (ou l'importer).
4. Dans `settings.html` : langue `fr_FR`, fuseau `Europe/Paris`, nom mDNS, coordonnées météo.
5. Charger la playlist (`playlist.csv` : nom, URL, `0`, séparés par des tabulations), de deux façons :
   - **Par le WebUI** : page de réglage des stations, boutons *Ajouter, Supprimer, Remplacer, Fusionner, Exporter, Partager, enregistrer, Annuler*. *Remplacer* = la liste du fichier remplace l'ancienne (carte neuve) ; *Fusionner* = ajoute à l'existante ; *Exporter* = sauvegarde en CSV. ⚠️ **Il faut cliquer sur « enregistrer »** après un import ou une modification, sinon rien n'est conservé.
   - **Avant le flash** : fichier renommé `playlist.csv` dans `webradio\data\data\` puis `uploadfs` (à vérifier avec `dir webradio\data\data`). Attention : chaque `uploadfs` remplace alors la playlist de la carte par celle du dossier ; exporter et mettre à jour ce fichier après toute modification des stations.
6. Après une modification du seul code : l'étape 2 suffit. `uploadfs` seulement si `data\www` change. Fermer le moniteur série avant tout upload.

Si la radio demande d'importer les fichiers de `data\www`, c'est que `uploadfs` n'a pas été fait avant le premier démarrage (page de secours qui propose aussi d'importer des CSV). Quand `uploadfs` a été fait avant, cette page n'apparaît pas : le log indique `Loaded 17 files into PSRAM cache` dès le premier démarrage.

---

## 📻 Stations de test (`playlist.csv`)

| Station | URL | Remarque |
|---|---|---|
| France Inter | `https://icecast.radiofrance.fr/franceinter-midfi.mp3?id=radiofrance` | ✅ |
| FIP | `https://icecast.radiofrance.fr/fip-midfi.mp3?id=radiofrance` | ✅ |
| France Culture | `https://icecast.radiofrance.fr/franceculture-midfi.mp3?id=radiofrance` | ✅ |
| France Musique | `https://icecast.radiofrance.fr/francemusique-midfi.mp3?id=radiofrance` | ✅ |
| Groove Salad | `https://ice1.somafm.com/groovesalad-128-mp3` | ✅ (« slow stream » dans le log) |
| Secret Agent | `https://ice1.somafm.com/secretagent-128-mp3` | ✅ (« slow stream ») |
| Drone Zone | `https://ice1.somafm.com/dronezone-128-mp3` | ✅ |
| Deep Space One | `https://ice1.somafm.com/deepspaceone-128-mp3` | ✅ (« slow stream ») |

Test du 5 nov. 2026 (log série) : les 8 stations se connectent, le décodeur MP3 démarre et les titres s'affichent. Handshake TLS : environ 320 ms pour Radio France, 700 à 1100 ms pour SomaFM. Le message `slow stream, dropouts are possible` apparaît sur SomaFM : à surveiller à l'écoute (coupures possibles).

Les adresses `https://ice.somafm.com/<chaîne>` n'ont **pas** fonctionné : utiliser la forme `iceN.somafm.com/<chaîne>-128-mp3`.

---

## 🔮 À faire

- [ ] **Tester le son** avec le PCM5102 (câblage ci-dessus, volume bas au départ). En cas de silence : ponts au dos du module, fils DIN/BCK/LCK, volume du WebUI, log série.
- [ ] **Sauvegarde Git** : `webradio/` est « untracked » dans le dépôt parent. Probablement un `.git` imbriqué (hérité du clone d'ehRadio) :
  1. `Test-Path webradio\.git`
  2. `git -C webradio rev-parse HEAD` (noter la version d'ehRadio de départ)
  3. `Set-Content webradio\.gitignore ".pio/`n.vscode/"`
  4. `Remove-Item -Recurse -Force webradio\.git`
  5. `git add webradio`, `git commit`, `git push`
- [ ] Écouter les stations SomaFM pour voir si le « slow stream » provoque des coupures.
- [ ] Surveiller la **RAM interne** : libre ~125 Ko au repos, ~80 Ko pendant un flux HTTPS, mais minimum de **13,5 Ko** relevé (`[Heap] Free: 80000 Min: 13576`) après avoir enchaîné les 8 stations rapidement (poignées de main TLS). Éviter de changer de station en rafale ; si un plantage apparaît, regarder d'abord de ce côté.
- [ ] Signaler le bug du `NULL` à trip5/ehRadio (éventuellement via un fork).
- [ ] Régler l'orientation de l'écran et du tactile si besoin (« Inverser l'écran », « Inverser tactile » dans `settings.html`).
- [ ] **Radio de chevet** (réveil, alimentée en USB, écran plus grand) : voir « Pistes ».

---

## 🧭 Pistes étudiées (en pause)

- **Radio de chevet** : MAX98357A (DAC + ampli I2S, enceinte 4 Ω, mêmes fils 16/17/15) ou PCM5102 + ampli. Alimentation USB 5 V / 2 A. Il faudra une broche PWM pour la luminosité (`BRIGHTNESS_PIN`, `DSP_DIMMING_ENABLED`), et vérifier si cette version d'ehRadio a un réveil ou une minuterie d'endormissement.
- **Écran 480x320** : ehRadio gère ILI9488 et ST7796 ; contrôleur à confirmer sur le matériel.
- **Waveshare ESP32-S3-RGB-Matrix** (carte de la pendule v5) : **les broches HUB75 ne conviennent pas** pour un écran SPI (derrière les SN74HC245, sortie à sens unique). Le connecteur GPIO libre est inutilisable (IO45/IO46). Elle a un ES8311 + ampli + J2 : option « ehRadio sans écran » ou « mode radio dans la v5 » (ESP32-audioI2S sait lire des flux web, mais la RAM interne est très juste et il faudrait Wi-Fi AP+STA).
- **Waveshare ESP32-Touch-LCD-3.5** : carte tout-en-un (ESP32 d'origine, 2 Mo PSRAM, 16 Mo flash, ST7796 320×480, tactile FT6336, ES8311 + connecteur haut-parleur, RTC, microSD, batterie). Compatible sur le papier avec ehRadio, mais **brochage à récupérer** sur le wiki Waveshare avant de préparer une config.
- **Module DFRobot DFR0781 / DFR0782** (et BT401) : récepteurs Bluetooth, sortie **analogique**, pas d'entrée I2S → **inutilisables** comme DAC d'ehRadio. L'ESP32-S3 n'a pas de Bluetooth Classique (pas d'A2DP).

---

*PIGTEAM 2026 🐷*
