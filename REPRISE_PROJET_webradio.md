# 🔄 REPRISE PROJET — Web-radio ehRadio (ESP32-S3 N16R8)

> Document de reprise rapide. En cas de nouvelle session, lire ce fichier en premier.
> Dernière mise à jour : 5 novembre 2026 au soir (son PCM5102 validé, recherche Radio-Browser réparée, horloge 7 segments).

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
| OLED 1,3" I2C SH1106 (128×64) | ✅ validé (`ECRAN_OLED 1`, du premier coup) |
| Tactile XPT2046 (glissement = volume) | ✅ validé |
| Connexion aux flux (4 Radio France + SomaFM) | ✅ validé |
| **Sortie son PCM5102** | ✅ **validé** (5 nov. soir, sur breadboard : contacts fragiles au début, puis OK) |
| **Recherche de stations Radio-Browser** (`search.html`) | ✅ **réparée** (bibliothèque `ESPFileUpdater` corrigée, voir « Correctifs ») |
| Horloge 7 segments (`CLOCKFONT YO_MONO`) | ⏳ ajoutée à `myoptions.h`, à juger à l'écran |
| Luminosité réglable de l'écran | ❌ `BRIGHTNESS_PIN` non défini (LED branchée sur 3,3 V) |
| Sauvegarde Git de `webradio/` | ✅ dépôt privé ; `myoptions.h` et `platformio.ini` à ajouter avec `git add -f` (voir « Git — mémo ») |

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

**OLED 1,3" I2C** (SH1106, 128×64, sans tactile ; `ECRAN_OLED 1`) :

| OLED | ESP32-S3 |
|---|---|
| SDA | GPIO 4 |
| SCL | GPIO 5 |
| VCC / GND | 3,3 V / GND |

**Validé** : `DSP_SH1106` + adresse `0x3C` par défaut fonctionnent sans rien changer. Pour un autre module : adresse `0x3D` possible (réglage `SCREEN_ADDRESS` dans `options.h`). Si l'image est décalée ou brouillée : `DSP_SSD1306` à la place de `DSP_SH1106`. Pas de tactile : commande par le WebUI.

**DAC PCM5102 (I2S)** :

| PCM5102 | ESP32-S3 |
|---|---|
| DIN | GPIO 16 |
| BCK | GPIO 17 |
| LCK | GPIO 15 |
| VIN / GND | 3,3 V / GND |
| SCK, FLT, DMP, FMT | GND |
| XMT (XSMT) | **3,3 V** (à la masse = son coupé) |

Ponts de soudure au dos du module (si on soude au lieu de câbler) : 1 (FLT) = **L**, 2 (DEMP) = **L**, 3 (XSMT) = **H**, 4 (FMT) = **L**. Le pont 3 sur L couperait le son. Validé ici avec des fils (les ponts étaient trop petits à souder).

⚠️ N16R8 : **GPIO 26 à 37 interdits** (flash et PSRAM octale).
Le PCM5102 sort un niveau ligne : il faut une enceinte amplifiée, un ampli ou un casque. Volume de l'enceinte bas au premier essai.

---

## 📋 Fichiers de configuration

Les fichiers de config vivent à la racine du projet. Dernière version : zip `ehradio_fichiers_v12` (voir ci-dessous).

`myoptions.h` — une ligne `#define ECRAN_OLED` choisit l'écran : `1` = OLED 1,3" I2C (SH1106), `0` = TFT ILI9341 + tactile (version de la maison) :

```c
#ifndef myoptions_h
#define myoptions_h

#define ECRAN_OLED 0   /* 1 = OLED 1,3" I2C, 0 = TFT ILI9341 + tactile */

#if ECRAN_OLED
  #define DSP_MODEL     DSP_SH1106   /* si image décalée/brouillée : DSP_SSD1306 */
  #define I2C_SDA       4
  #define I2C_SCL       5
#else
  #define DSP_MODEL     DSP_ILI9341
  #define SPIA_DEFAULT                  /* SCK=12, MISO=13, MOSI=11 */
  #define TFT_CS        10
  #define TFT_DC         9
  #define TFT_RST        8
  #define TS_MODEL      TS_MODEL_XPT2046
  #define TS_SPI        'A'
  #define TS_CS         14
#endif

#define I2S_DOUT      16
#define I2S_BCLK      17
#define I2S_LRC       15
#define VS1053_CS     255

#define SEARCHRESULTS_BUFFER 32   /* tampon de la recherche Radio-Browser (PSRAM) */

/* Police de l'horloge : YO_MONO = 7 segments (façon yoRadio). Autres : CHUNKY6_PX (défaut), CHUNKY6 */
#define CLOCKFONT YO_MONO
#define CLOCKGLOW true

#endif
```

`platformio.ini` — environnement (les sections `[ehradio]` et `[library]` sont celles du dépôt ehRadio). **`ESPFileUpdater` n'est plus dans `lib_deps` : la version corrigée est dans `lib\ESPFileUpdater\` (dans le projet, donc dans Git)** :

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
  bblanchon/ArduinoJson@^7.4.3
  adafruit/Adafruit GFX Library@^1.12.6
  adafruit/Adafruit BusIO@^1.17.4
  ESP32Async/ESPAsyncWebServer@^3.11.2
  ESP32Async/AsyncTCP@^3.4.10
  marvinroger/AsyncMqttClient@^0.9.0
  trip5/ehDP@^1.0.0
  jnthas/Improv WiFi Library@^0.0.4
  ${library.ili9341}
  ${library.sh110x}
  https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
```

Si ehRadio change la liste de `[ehradio].lib_deps` lors d'une mise à jour de trip5, recopier les nouvelles lignes ici (sauf `trip5/ESPFileUpdater`).

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
7. **Recherche Radio-Browser en échec** (`Chunked transfer framing invalid or truncated`, coupure vers 4090 octets) : ce n'était **ni le réseau, ni la mémoire, ni le serveur**. Avec le cœur Arduino-ESP32 3.3.x, `NetworkClientSecure::read()` renvoie -1 quand aucune donnée n'est encore arrivée, et `NetworkClient::readBytes()` prend ce -1 pour une erreur et rend la main sans attendre : la bibliothèque `ESPFileUpdater` 1.5.0 croit le flux coupé après le premier paquet TLS. **Correctif** : dans `lib\ESPFileUpdater\src\ESPFileUpdater.cpp`, fonctions `readByteWait()` (remplace `stream.read()`) et `readSome()` (remplace les 5 `stream.readBytes()`), qui attendent vraiment les données. Résultat : 125 903 octets téléchargés et vérifiés. Fausses pistes écartées : alias `all.api` (de1 aussi marche), `SEARCHRESULTS_BUFFER`, réseau du travail. À signaler à trip5 (texte de l'issue dans `ISSUE_trip5_ESPFileUpdater.md`).
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
7. **Commandes de flash de toutes les cartes** (N16R8, Tiny, Zero) : voir `CARTES_Tiny_Zero_commandes.md`, section « Commandes de flash ». Moniteur série : `pio device monitor -e ehradio_s3_n16r8`. Carte non reconnue : maintenir BOOT en branchant l'USB.

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

- [x] **Son PCM5102 validé** (5 nov. 2026).
- [x] **Recherche Radio-Browser réparée** (voir Correctifs, n°7).
- [ ] Juger l'horloge 7 segments (`CLOCKFONT YO_MONO`) ; sinon `CHUNKY6_PX`. Les thèmes ne changent que les couleurs ; la disposition (`layout`) a 2 choix sur 320×240 (« Default », « VaraiTamas (BoomBox) »).
- [ ] **Boutons / encodeur rotatif** (modules OLED avec rotatif + 2 poussoirs à la maison) : `BTN_*`, `ENC_CLK/DT/SW`, libs `onebutton` et `rotaryencoder` à décommenter. Avant : `findstr /s /n /i "BTN_PREV BTN_UP" src\core\*.cpp`.
- [ ] Envoyer l'issue à trip5 (`ISSUE_trip5_ESPFileUpdater.md`) et signaler aussi le pointeur nul (`settings.html`) et la faute `SNTP_1`.
- [x] **Sauvegarde Git** faite (dépôt privé dédié, voir « Git — mémo »).
- [x] **OLED 1,3" validé** (5 nov. 2026) : fonctionne du premier coup avec `DSP_SH1106`, adresse `0x3C` par défaut, SDA = GPIO 4, SCL = GPIO 5. Aucun réglage supplémentaire.
- [ ] Écouter les stations SomaFM pour voir si le « slow stream » provoque des coupures.
- [ ] Surveiller la **RAM interne** : libre ~125 Ko au repos, ~80 Ko pendant un flux HTTPS, mais minimum de **13,5 Ko** relevé (`[Heap] Free: 80000 Min: 13576`) après avoir enchaîné les 8 stations rapidement (poignées de main TLS). Éviter de changer de station en rafale ; si un plantage apparaît, regarder d'abord de ce côté.
- [ ] Régler l'orientation de l'écran et du tactile si besoin (« Inverser l'écran », « Inverser tactile » dans `settings.html`).
- [ ] **Radio de chevet** (réveil, alimentée en USB, écran plus grand) : voir « Pistes ».

---

## 🔧 Git — mémo

**État** : dépôt **privé** sur GitHub (compte perso), basé sur un clone d'ehRadio. Deux « remotes » : `origin` = ton dépôt, `upstream` = `trip5/ehRadio` (pour récupérer ses mises à jour). `myoptions.h` et `platformio.ini` sont ignorés par le `.gitignore` d'ehRadio : ils ont été ajoutés **une fois de force** (`git add -f`), ensuite ils sont suivis normalement. `.github/` et `.roo/` ont été supprimés volontairement (workflows de trip5 inutiles chez toi). Les playlists sont dans `playlists/`.

**Routine à connaître** (PowerShell, dans le dossier du projet) :

| Commande | Rôle |
|---|---|
| `git status` | Voir ce qui a changé |
| `git diff fichier` | Voir le détail des changements d'un fichier |
| `git add -A` | Préparer tous les changements |
| `git commit -m "message"` | Enregistrer en local avec un message |
| `git push` | Envoyer sur GitHub |
| `git pull` | Récupérer depuis GitHub (autre PC) |
| `git log --oneline` | Historique |
| `git restore fichier` | **Annuler** les changements non enregistrés d'un fichier (irréversible) |
| `git clone <adresse>` | Récupérer le projet sur un nouveau poste |
| `git fetch upstream` puis `git merge upstream/main` | Mises à jour de trip5 (conflits possibles sur `netserver.cpp`) |

**Séquence habituelle après une modification** : `git status`, `git add -A`, `git commit -m "ce que j'ai changé"`, `git push`.

**Pièges** :
- Si Git refuse de commiter faute d'identité : `git config user.name "ton nom"` et `git config user.email "ton adresse"` (sans `--global`).
- Sur un poste qui n'est pas le tien : se déconnecter ensuite du Gestionnaire d'identification Windows.
- Les copies `.bak` ne sont pas ignorées : elles apparaissent dans `git status`.
- Après un `git clone` sur un nouveau poste : refaire `uploadfs` puis `upload` sur la carte.
- Avant d'essayer un nouveau réglage : `git commit` d'abord, pour pouvoir revenir (`git restore`).

---

## 🖥️ Écrans pris en charge (lu dans `options.h`)

| Écran | Résolution |
|---|---|
| LCD 1602 / 2004 (caractères) | 16×2 / 20×4 |
| Nokia 5110 | 84×48 (plus petit graphique) |
| GC9106 | 160×80 |
| ST7735 | 160×128, 128×128 ou 160×80 |
| SH1106, SH1107, SSD1305, SSD1306, SSD1327, ST7920 | 128×64 |
| ILI9225 | 220×176 |
| GC9A01A | 240×240 rond |
| SSD1322 | 256×64 |
| ILI9341 (fixe), ST7789 | 320×240 |
| ILI9488, ILI9486, ST7796 | 480×320 (plus grand) |

- `DSP_WIDTH` / `DSP_HEIGHT` ajustent la résolution de certains modèles. Pour du 480×320, le ST7796 est préférable : le code traite ILI9488 et ILI9486 comme lents (playlist par pages).
- **Luminosité** : `DSP_DIMMING_ENABLED`, `SCREEN_BRIGHTNESS`, `DIMMING_BRIGHTNESS` n'agissent que si `BRIGHTNESS_PIN` est défini.
- **Valeurs par défaut modifiables dans `myoptions.h`** (pour qu'un ESP32 neuf démarre déjà configuré) : `TIMEZONE_NAME`, `TIMEZONE_POSIX`, `WEATHER_LAT`, `WEATHER_LON`, `DSP_LOCALE`, `WEBUI_LOCALE`. Par défaut : Canada/Atlantic et Halifax.
- **Faute dans `options.h`** : `SNTP_1` vaut `"0.pool.ntp.or"` (il manque le « g ») ; `SNTP_2` est correct, l'heure fonctionne donc. À signaler à trip5 avec le bug du pointeur nul.
- `#define RADIO_BROWSER_NO_SEND_CLICKS` coupe les `[RB Click]` (comptage des écoutes sur Radio-Browser).

---

## 🧭 Pistes étudiées (en pause)

- **Sculpture de circuit** (style Mohit Bhoite / Eirik Brandal) : laiton Ø 1 mm commandé. Soudure : poncer, flux, fer 350–380 °C, étamer avant d'assembler, gabarit imprimé en 3D pour tenir les pièces. Jamais deux tiges nues qui se croisent sans soudure (écart 3–4 mm ou fil émaillé). Masse et 3,3 V sur des tiges séparées. Garder l'USB-C et BOOT/RESET accessibles. Valider tout sur breadboard avant. Son : PCM5102 + enceinte amplifiée pour l'instant.
- **Module autoradio** (idée) : boîtier imprimé simple, PCM5102 → prise jack 3,5 mm vers l'entrée AUX ; prévoir convertisseur 12 V → 5 V propre et un isolateur de boucle de masse (sinon ronflement). Cartes compactes possibles : ESP32-S3-Zero, ESP32-S3-Tiny (N8R8).

- **Radio de chevet** : MAX98357A (DAC + ampli I2S, enceinte 4 Ω, mêmes fils 16/17/15) ou PCM5102 + ampli. Alimentation USB 5 V / 2 A. Il faudra une broche PWM pour la luminosité (`BRIGHTNESS_PIN`, `DSP_DIMMING_ENABLED`), et vérifier si cette version d'ehRadio a un réveil ou une minuterie d'endormissement.
- **Écran 480x320** : ehRadio gère ILI9488 et ST7796 ; contrôleur à confirmer sur le matériel.
- **Waveshare ESP32-S3-RGB-Matrix** (carte de la pendule v5) : **les broches HUB75 ne conviennent pas** pour un écran SPI (derrière les SN74HC245, sortie à sens unique). Le connecteur GPIO libre est inutilisable (IO45/IO46). Elle a un ES8311 + ampli + J2 : option « ehRadio sans écran » ou « mode radio dans la v5 » (ESP32-audioI2S sait lire des flux web, mais la RAM interne est très juste et il faudrait Wi-Fi AP+STA).
- **Waveshare ESP32-Touch-LCD-3.5** : carte tout-en-un (ESP32 d'origine, 2 Mo PSRAM, 16 Mo flash, ST7796 320×480, tactile FT6336, ES8311 + connecteur haut-parleur, RTC, microSD, batterie). Compatible sur le papier avec ehRadio, mais **brochage à récupérer** sur le wiki Waveshare avant de préparer une config.
- **Module DFRobot DFR0781 / DFR0782** (et BT401) : récepteurs Bluetooth, sortie **analogique**, pas d'entrée I2S → **inutilisables** comme DAC d'ehRadio. L'ESP32-S3 n'a pas de Bluetooth Classique (pas d'A2DP).

---

*PIGTEAM 2026 🐷*
