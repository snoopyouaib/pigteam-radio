# 🎛️ Cartes Tiny / S3-Zero TFT — config et commandes (ehRadio)

> Complément de `REPRISE_PROJET_webradio.md` (qui décrit la N16R8). Dernière mise à jour : 13 novembre 2026 (versions 4 Mo abandonnées et retirées) (ajout Tiny + ILI9488, correctif `uploadfs`).
> Dépôt : `D:\vscode_projets\ehradio-pigteam` (privé, GitHub `snoopyouaib/ehradio-pigteam`, remotes `origin` + `upstream`). Un futur dépôt privé « radio-pigteam » est prévu (garder l'historique : `git remote rename origin ancien`, `git remote add origin ...`, `git push -u origin --all`, `git push origin --tags`). Pas de distribution prévue : usage perso (GPL v3 non contraignante sans diffusion).

## 📦 État

| Carte | Env PlatformIO | État |
|---|---|---|
| ESP32-S3-Tiny N8R8 (8 Mo flash, PSRAM **octale** 8 Mo) + ST7789 320×240 + Lipo Rider Plus | `ehradio_s3_tiny_tft` | ✅ démarre, Wi-Fi, WebUI, fr_FR, Europe/Paris, météo FR, **son PCM5102 OK**, **son MAX98357 OK (en 3,3 V)**, commandes rotatif OK, **SD configurée (testée OK)**, **logo PigTeam rond au boot**, filtre des titres bidons OK, pas de tactile (normal) |
| ESP32-S3-Tiny N8R8 + **ILI9488 480×320 tactile** + SD + PCM5102 (même firmware que la N16R8 : réveil, arrêt programmé, diaporama, menu tactile) | `ehradio_s3_tiny_ili9488` | ✅ **tout validé le 12 nov. 2026** : écran + tactile, Wi-Fi, WebUI, `uploadfs` non compressé, **son MAX98357A**, **SD** (21 MP3, mode SD) + **diaporama** (174 photos 480×320, ~330 ms chacune), **luminosité** (PWM GPIO 6), **arrêt programmé** et **réveil** (montée 0→25 en 30 s) ; reste la batterie (GPIO 9) |
| ESP32-S3 N16R8 + ILI9341 / OLED | `ehradio_s3_n16r8` | ✅ voir doc de reprise |

Un seul `myoptions.h` pour toutes les cartes : `-DCARTE_TINY_TFT`, `-DCARTE_TINY_ILI9488` (flags de l'env) choisissent la branche ; sans flag = n16r8 (`ECRAN_OLED 0/1`). VS Code grise les branches inactives : c'est normal (ce ne sont pas des commentaires).

Cartes en stock (inventaire) : Waveshare ESP32-S3-DEV-KIT en N32R16V (module **WROOM-2** : flash et PSRAM **octales**, mode `opi_opi` + `flash_mode = opi` à prévoir, GPIO 26–37 interdits ; rien de préparé) ; un N8R8 du même modèle suffirait (même mémoire que la Tiny, `qio_opi`, `default_8MB.csv`).

## 🔌 Tiny TFT — brochage

| Fonction | GPIO |
|---|---|
| ST7789 SCK / MOSI / MISO | 11 / 12 / 255 (pas de MISO) |
| TFT CS / DC / RST | 15 / 14 / 13 |
| Rétroéclairage (PWM, `DSP_DIMMING_ENABLED`) | 16 |
| Batterie (`BATTERY_DIVIDER_RATIO 2.0`) | 1 |
| Rotatif CLK / DT / SW | 17 / 18 / 44 |
| Bouton BACK (`BTN_PLAY`) | 43 |
| Carte SD (bus SPI **B**, `SD_SPI 'B'`) CS / MOSI / MISO / SCK | 10 / 7 / 8 / 9 |
| I2S (PCM5102 ou MAX98357) DIN / BCK / LCK | 4 / 5 / 6 |

GPIO 43/44 = UART0 TX/RX (d'où le re-armementGPIO 43/44 = UART0 TX/RX (d'où le re-armement des broches, voir ci-dessous). 0 et 3 = strapping, 19/20 = USB. GPIO 2 et 3 libres.

## 🔌 Tiny + ILI9488 — brochage (env `ehradio_s3_tiny_ili9488`, 12 nov. 2026)

Écran 4" 480×320 ILI9488 + tactile XPT2046 + SD du module + PCM5102, sans rotatif ni BACK (tout au tactile). Écran sur **GPIO 1 à 8** dans l'ordre des broches du module (câblage facile) ; branche `CARTE_TINY_ILI9488` de `myoptions.h`.

| Module | GPIO | Remarque |
|---|---|---|
| CS / RST / D/C | 1 / 2 / 3 | |
| SDI (+ TDI) | 4 | MOSI ; T_DIN relié à SDI par un pont sur le module |
| SCK (+ TCK) | 5 | T_CLK relié à SCK par un pont sur le module |
| BL (LED) | 6 | PWM, transistor Q1 du module |
| TCS | 7 | T_CS |
| TDO | 8 | MISO du tactile (seule ligne MISO) |
| SDO, TCK, TDI, PEN | non branchés | SDO : l'ILI9488 ne libère pas la ligne ; TCK/TDI déjà doublés ; PEN (T_IRQ) inutile |
| SD du module (bus SPI B) SCK / MOSI / MISO / CS | 15 / 12 / 13 / 14 | SCK déplacé de 11 à 15 pour grouper les 4 fils du même côté |
| PCM5102 ou MAX98357A DIN / BCK / LCK | 16 / 17 / 18 | MAX98357A validé (même câblage) |
| Batterie (optionnelle, ×2) | 9 | ADC1 : l'ADC2 (GPIO 11 à 20) est inutilisable avec le Wi-Fi |

Libres : 10, 11, 43/44. Interdits : 33–37 (PSRAM octale), 19/20 (USB). Flash : `-e ehradio_s3_tiny_ili9488`, envoi non compressé déjà configuré dans l'env (voir piège n°6).

## 🎮 Commandes (sans tactile)

- **Tourner** : volume.
- **Clic rotatif** : ouvre la liste des stations ; dans la liste, tourner = choisir, clic = valider et sortir. (Marche aussi juste après un réglage de volume.)
- **Double-clic rotatif** : lecture / arrêt.
- **BACK (GPIO 43)** : lecture / arrêt (clic), appui long = bascule lecteur ↔ liste.
- `#define ENC_CLICK_OPENS_LIST` dans `myoptions.h` active ce schéma.
- Avec la SD active : maintenir un bouton au démarrage (clic rotatif ou BACK) = mode « SD offline » (sans Wi-Fi). Sans Wi-Fi, ce mode prend aussi le relais.
- WebUI « Accélération encodeur » (0 à 7, défaut en place : 3) : accélération du **volume** quand on tourne vite ; sans effet sur la liste des stations (1 station par cran). 0 = linéaire.

### Patchs dans les fichiers d'ehRadio (tous dans le dépôt)

`src/core/controls.cpp` :
1. `onBtnClick` : sous `ENC_CLICK_OPENS_LIST`, clic de `EVT_ENC_SW` en mode PLAYER → `NEWMODE STATIONS` (sinon `player.toggle()`). En mode VOL, clic de `EVT_ENC_SW` → STATIONS directement.
2. `onBtnDoubleClick` : double-clic `EVT_ENC_SW` → `player.toggle()` (PLAYER ou VOL).
3. `Controls::init` : `rearmBtnPin()` (`gpio_reset_pin` + `pinMode`) pour chaque bouton, pour libérer GPIO43 de la fonction UART0. BACK marche depuis ce correctif (cause exacte non prouvée).
4. `controlsEvent`, branche STATIONS : anti-doublon 80 ms (`lastStep`) → 1 cran = 1 station (l'EC11 envoyait 2 événements par cran).

`src/core/audiohandlers.cpp` : fonction `isNumericJunk()` (titre uniquement chiffres, espaces et tirets) ajoutée au-dessus de `handleShowStreamTitle`, et ligne `bool printable = isPrintable(info) && (strlen(info) > 0) && !isNumericJunk(info);`. Raison : Nostalgie/NRJ envoient des `StreamTitle` bidons (`999999`, `9975 - 9975`, `2045 - 2045`) ; on affiche alors le nom de la station. (Ne pas toucher à `handleShowStation`, qui a un `printable` identique mais sans rapport.)

`src/displays/dspfont.h` : première branche `#if defined(PIGTEAM_LOGO) && DSP_HEIGHT >= 176` → `#include "bootlogo/pigteam.h"`.
`src/displays/bootlogo/pigteam.h` (nouveau) : logo PigTeam « Fairy Pig », **rond 128×128** RGB565, disque blanc / extérieur noir, `LOGO_TOP 12`. Généré depuis l'image 600×450 « The Pig Team Conception » (projet chrono).
`src/core/display.cpp` (`Display::_bootScreen`) : `#ifdef LOGO_TOP dsp.drawLogo(LOGO_TOP); #else dsp.drawLogo(BOOTLOGOTOP); #endif`.
`myoptions.h` : `#define PIGTEAM_LOGO` (bloc commun, avant `CLOCKFONT`) → logo actif sur tous les écrans ≥ 176 px de haut (Tiny, N16R8 ILI9341) ; les OLED 128×64 gardent le logo d'origine.

⚠️ **`ENC_STEPS` n'accepte que 1, 2 ou 4** (`#error` sinon, `options.h:581`). Valeur retenue : **4**. Ne pas mettre 8.

À refaire après une mise à jour d'ehRadio (trip5) : ces patchs écrasent `controls.cpp`, `audiohandlers.cpp`, `dspfont.h` et `display.cpp` (le commit git permet de les retrouver).

## 🖥️ Réglages d'affichage

- **Vitesse de défilement** : `#define SCROLLTIME 30` dans `myoptions.h` (retenu ; défaut 20 ms par pas = ~100 px/s sur 320×240 ; commun à tous les textes qui défilent). Pour ralentir la météo seule : dernier argument de `.weatherConf` dans `src/displays/conf/displayTFT320x240conf.h` (`SCROLLTIME*2`).
- WebUI « Tampon audio » (`bufferbar`) : barre de remplissage du tampon audio (diagnostic des coupures). « Page volume » (`volumepage`) : écran dédié au volume quand on tourne le rotatif.
- Mise en page « VaraiTamas (BoomBox) » + horloge 7 segments (`CLOCKFONT YO_MONO`) : retenues (la mise en page est un réglage WebUI, pas dans `myoptions.h`).
- **Coordonnées météo (WebUI)** : virgule comme séparateur et **4 chiffres maximum après la virgule**, sinon la saisie est refusée. Dieppe : `49,9229` / `1,0775`. Défauts modifiables via `WEATHER_LAT` / `WEATHER_LON` dans `myoptions.h`.
- Les paramètres du WebUI ne s'exportent pas (seuls la playlist et `wifi.csv` ont un bouton « Exporter ») ; ils sont dans la NVS. Pour les retrouver sur carte neuve : mettre les valeurs voulues comme défauts dans `myoptions.h` (`TIMEZONE_NAME`, `TIMEZONE_POSIX`, `WEATHER_LAT`, `WEATHER_LON`, `DSP_LOCALE`, `WEBUI_LOCALE`, `ROTARY_ACCEL`, `SCREEN_BRIGHTNESS`…). Un export/import complet nécessiterait un patch `netserver.cpp` (non fait).

## 📻 Playlist (WebUI)

- Colonnes de l'éditeur : nom, URL, **Ovol** = ajustement de volume par station (0 = aucun ; un nombre plus grand = station jouée plus fort ; sert à égaliser les niveaux entre radios).
- **Trier** : faire glisser le numéro (001, 002, …) à gauche de chaque ligne ; puis **Enregistrer** (sinon rien n'est conservé) ; « Annuler » revient en arrière. Tri alphabétique : exporter le CSV, trier sur PC, réimporter avec « Remplacer ».
- Format `playlist.csv` : tabulations, `nom<TAB>URL<TAB>ovol`.
- Choix des flux : MP3 128 kbps = le plus sûr ; AAC/AAC+ 64–128 kbps OK (le décodeur AAC avec SBR tourne sans problème, mémoire libre quasi identique) ; éviter > 192–256 kbps, FLAC, Opus.
- Sauvegarder les exports (playlist + `wifi.csv`) dans le dépôt (`playlists\`).

## 🔋 Batterie (Lipo Rider Plus) et alimentation

`BATTERY_PIN 1`, diviseur 10k/10k (×2). La section « Batterie » du WebUI est vide tant qu'aucune LiPo n'est branchée. Une fois branchée (⚠️ **vérifier la polarité du JST** avant) : démarrer sans USB, mesurer la tension aux bornes (mV), saisir la valeur dans « Lecture multimètre » et cliquer « étalonner ». Le pourcentage reste approximatif ; l'état de charge n'est pas lu (LED du Lipo Rider Plus). Pour masquer la section : `#define BATTERY_PIN 255`.

Alimentation : sans module batterie, le 5 V vient du port USB-C (broche 5V/VBUS de la carte si elle est sortie — à vérifier dans le README de la Tiny). La broche 5V est en général le même point que le 5 V USB (souvent via diode) : **ne pas brancher l'USB en même temps qu'une alimentation externe sur cette broche** sans être sûr d'une diode. Sinon, le MAX98357 peut rester sur le 3,3 V de la carte à volume modéré.

## 🚀 Commandes de flash (toutes les cartes)

Dans `D:\vscode_projets\pigteam-radio` (PowerShell), **moniteur série fermé**. Remplacer `<env>` :

| Carte | `<env>` |
|---|---|
| ESP32-S3 N16R8 (ILI9488 / ILI9341 / OLED + PCM5102) | `ehradio_s3_n16r8` |
| ESP32-S3-Tiny N8R8 + ILI9488 tactile | `ehradio_s3_tiny_ili9488` |
| ESP32-S3-Tiny N8R8 + ST7789 + rotatif | `ehradio_s3_tiny_tft` |
| ESP32-S3-Zero N8R8 + ILI9488 tactile | `ehradio_s3_zero_tft` |

```powershell
pio run -e <env> -t uploadfs          # 1. pages web (carte neuve, ou si data\www a change) - D'ABORD
pio run -e <env> -t upload            # 2. firmware (seul, pour un changement de code)
pio device monitor -e <env>           # 3. moniteur serie (Ctrl+C pour quitter)
```

- `uploadfs` **ecrase** `wifi.csv` et `playlist.csv` de la carte par ceux de `data\` (les garder a jour, exports dans `playlists\`).
- Carte non reconnue : maintenir **BOOT** en branchant l'USB-C (ou BOOT + RESET), puis relancer ; `--upload-port COMx` si PlatformIO se trompe de port (le port change parfois en mode BOOT).
- Apres un reflash : SAFE MODE jusqu'a 30 s de fonctionnement stable ; lancer la lecture a la main.
- Tiny : l'envoi est **non compresse** (script `builds\platformio_tiny_no_compress.py`, voir piege n°6 ci-dessous) ; en cas d'echec de `uploadfs`, commande directe : `pio pkg exec -p tool-esptoolpy -- esptool --chip esp32s3 --port COMx write-flash --no-compress 0x670000 .pio\build\<env>\spiffs.bin`.
- Effacer la partition de fichiers (firmware et reglages conserves) : `pio pkg exec -p tool-esptoolpy -- esptool --chip esp32s3 --port COMx erase-region 0x670000 0x180000` (partitions 8 Mo ; pour le N16R8, voir `default_16MB.csv`).

## 🧱 Pièges de compilation / flash (Tiny)

1. **Octal PSRAM** : `board_build.arduino.memory_type = qio_opi` (le N8R8 Tiny). Avec `qio_qspi` : `PSRAM: 0MB`, pas de serveur web, 4 Ko de heap libre.
2. **Partitions** : `default_8MB.csv` (Tiny), `default_16MB.csv` (N16R8). Toujours préciser `-e <env>` (un mauvais env → « does not fit in flash » ou `uploadfs` en échec).
3. **Lib SD** : Adafruit ST7789 dépend de `SD` → PlatformIO installe le SD 1.3.0 du registre (incompatible ESP32-S3). Ligne dans `lib_deps` : `SD=file://${platformio.packages_dir}/framework-arduinoespressif32/libraries/SD`, puis supprimer `.pio\libdeps\ehradio_s3_tiny_tft\SD`.
4. **ESPFileUpdater** : version corrigée dans `lib\ESPFileUpdater\` (pas dans `lib_deps`), voir doc de reprise n°7.
5. Nouvelle carte : `uploadfs` **d'abord**, puis `upload`. `uploadfs` n'est utile que si `data\www` change ; **ne pas le lancer pour appliquer un patch de code** (`-t upload` suffit).
6. **`uploadfs` en USB échoue sur la Tiny** à 74,2 % (« The chip stopped responding »), toujours au même endroit, même à 230400 bauds, sans ampli, autre câble et autre port. La partition de fichiers reste alors à moitié écrite : la radio affiche la **page de secours** (« Outil de téléchargement de fichiers via l'interface Web ») et peut la reperdre au redémarrage. Remède qui a marché : (a) effacer proprement la partition de fichiers (le firmware et les réglages ne sont pas touchés) :
   `pio pkg exec -p tool-esptoolpy -- esptool --chip esp32s3 --port COM16 erase-region 0x670000 0x180000` (sinon `erase_region`), moniteur série fermé ; (b) redémarrer, ouvrir la page de secours, envoyer **tous** les fichiers de `data\www\` (17, dont `rb_srvrs.json`) puis le CSV de playlist/Wi-Fi ; (c) vérifier au redémarrage : `Loaded 17 files into PSRAM cache`. **Correctif trouvé le 12 nov. 2026** : le plantage a lieu dans l'écriture **compressée** (`flash_defl_block`) ; `esptool … write-flash --no-compress 0x670000 .pio\build\<env>\spiffs.bin` passe (1,5 Mo en 15 s, hash vérifié, `Loaded 17 files into PSRAM cache` au démarrage). Réglage permanent : script `builds/platformio_tiny_no_compress.py` (remplace `-z` par `--no-compress` dans les flags d'esptool), appelé par `extra_scripts` dans les envs `ehradio_s3_tiny_ili9488` et `ehradio_s3_tiny_tft` (à vérifier au prochain `uploadfs` ; sinon la commande esptool directe ci-dessus). ⚠️ `upload_flags = --no-compress` ne marche **pas** : PlatformIO le place avant `write-flash` (erreur « No such option »). Le point de coupure variait (74 % puis 95 %) : ce n'est pas lié à un fichier. Hypothèse non prouvée : alimentation / liaison USB-Serial/JTAG. Le log du moniteur série : utiliser `pio device monitor -e ehradio_s3_tiny_tft`.

## 🔊 Sortie audio

**PCM5102** (validé) : 3 ponts de soudure au dos : FLT, DEMP, FMT sur **L** (GND) ; XSMT (pont 3) sur **H** (3,3 V), jamais GND. Sur la Tiny : DIN 4, BCK 5, LCK 6 (déconnecter le GPS de 5/6 avant). Sortie niveau ligne (enceinte amplifiée ou casque).

**MAX98357 (DAC + ampli I2S mono)** — **validé en 3,3 V (6 nov.)**, pour la version 100 % autonome. Mêmes fils I2S, **aucun changement de code** :

| MAX98357 | Tiny |
|---|---|
| DIN / BCLK / LRC | GPIO 4 / 5 / 6 |
| VIN | 3,3 V (test OK) ou 5 V (pleine puissance, ~3 W sur 4 Ω) |
| GND | GND |
| SD | libre (mélange L+R en mono) — vérifier la sérigraphie des clones |
| GAIN | libre = 9 dB, GND = 12 dB, VIN = 6 dB |

Haut-parleur 4 à 8 Ω. **Grésillements** constatés au début : causés par des contacts / fils mobiles (ils disparaissent en tenant ou en scotchant ensemble les fils DIN/BCLK/LRC/GND ; le condensateur 100 µF sur VIN/GND n'a rien changé). Solution : **souder** les fils, les garder courts et groupés (BCLK torsadé avec une masse). Pour la sculpture en tiges de laiton : signaux I2S courts, tige de masse à côté, écart 3–4 mm entre tiges nues (ou fil émaillé pour croiser), module MAX98357 collé à l'ESP32 ; haut-parleur et alimentation peuvent être plus longs. Valider la disposition à plat avec les mêmes longueurs avant de monter la sculpture.

**Carte son Waveshare WM8960 (HAT Raspberry Pi, en stock)** : **écartée**. ehRadio ne gère que le codec ES8311 (`USE_ES8311`) ; le WM8960 demande un pilote I2C (et peut-être une MCLK) qui n'existe pas dans ehRadio.

## 🖼️ Écrans en stock (idées, non testés)

- **ST7789 76×284** : mise en page ehRadio prévue (`displayTFT284x76conf.h`) ; pilote ST7789 existant ; `DSP_WIDTH 284` / `DSP_HEIGHT 76` dans `myoptions.h` ; risque de décalage de fenêtre à régler par essais (offsets).
- **LCD NV3007 142×428** : **non géré** (aucun pilote NV3007 dans ehRadio ; mise en page 428×142 marquée « untested »). Il faudrait ajouter un pilote (ex. Arduino_GFX).

## 🔮 À faire

- [x] `git add/commit/push` (SD, logo, SCROLLTIME, filtre des titres numériques — à refaire après chaque patch).
- [x] MAX98357 testé en 3,3 V.
- [ ] Souder le câblage du MAX98357, tester en 5 V ; version autonome (LiPo + Lipo Rider Plus + haut-parleur).
- [ ] Brancher une LiPo et étalonner la batterie ; tester la luminosité (PWM GPIO 16).
- [x] Tiny + ILI9488 : son, SD + diaporama, luminosité, réveil, arrêt programmé et `uploadfs` validés (12 nov. 2026).
- [ ] Tiny + ILI9488 : brancher une LiPo et étalonner la batterie (GPIO 9).
- [ ] Envoyer l'issue à trip5 (`ISSUE_trip5_ESPFileUpdater.md`).
- [ ] Sculpture de circuit en laiton (Ø 1 mm commandé) : idée de forme encore en réflexion ; objectif 1 ou 2 cadeaux pour Noël ; s'entraîner à souder le laiton (tiges, anneau, figure plate, puis 3D).
- [ ] Choisir les stations préférées ; exporter playlist + Wi-Fi dans le dépôt.
- [ ] Module autoradio (idée) : boîtier imprimé simple, voir doc de reprise (« Pistes »).
- [ ] Plus tard : dépôt privé « radio-pigteam » (option : fork allégé).

*PIGTEAM 2026 🐷*
