# 🎛️ Cartes Tiny TFT / S3-Zero OLED — config et commandes (ehRadio)

> Complément de `REPRISE_PROJET_webradio.md` (qui décrit la N16R8). Dernière mise à jour : 6 novembre 2026 (soir).
> Dépôt : `D:\vscode_projets\ehradio-pigteam` (privé, GitHub `snoopyouaib/ehradio-pigteam`, remotes `origin` + `upstream`). Tout ce qui suit est commité et poussé (6 nov.).

## 📦 État

| Carte | Env PlatformIO | État |
|---|---|---|
| ESP32-S3-Tiny N8R8 (8 Mo flash, PSRAM **octale** 8 Mo) + ST7789 320×240 + Lipo Rider Plus | `ehradio_s3_tiny_tft` | ✅ démarre, Wi-Fi, WebUI, fr_FR, Europe/Paris, météo FR, **son PCM5102 OK**, commandes rotatif OK, **SD configurée (testée OK)**, **logo PigTeam rond au boot**, pas de tactile (normal) |
| ESP32-S3-Zero 4 Mo (N4R2, PSRAM quad) + OLED SH1106 + rotatif + 2 boutons | `ehradio_s3_zero` | ⏳ config prête, pas encore testée (1 carte fonctionne, 2 peut-être abîmées par la chaleur) |
| ESP32-S3 N16R8 + ILI9341 / OLED | `ehradio_s3_n16r8` | ✅ voir doc de reprise |

Un seul `myoptions.h` pour les 3 : `-DCARTE_ZERO` et `-DCARTE_TINY_TFT` (flags de l'env) choisissent la branche ; sans flag = n16r8 (`ECRAN_OLED 0/1`). VS Code grise les branches inactives : c'est normal (ce ne sont pas des commentaires).

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

GPIO 43/44 = UART0 TX/RX (d'où le re-armement des broches, voir ci-dessous). 0 et 3 = strapping, 19/20 = USB. GPIO 2 et 3 libres.

## 🎮 Commandes (sans tactile)

- **Tourner** : volume.
- **Clic rotatif** : ouvre la liste des stations ; dans la liste, tourner = choisir, clic = valider et sortir. (Marche aussi juste après un réglage de volume.)
- **Double-clic rotatif** : lecture / arrêt.
- **BACK (GPIO 43)** : lecture / arrêt (clic), appui long = bascule lecteur ↔ liste.
- `#define ENC_CLICK_OPENS_LIST` dans `myoptions.h` active ce schéma.
- Avec la SD active : maintenir un bouton au démarrage (clic rotatif ou BACK) = mode « SD offline » (sans Wi-Fi). Sans Wi-Fi, ce mode prend aussi le relais.

### Patchs dans les fichiers d'ehRadio (tous dans le dépôt)

`src/core/controls.cpp` :
1. `onBtnClick` : sous `ENC_CLICK_OPENS_LIST`, clic de `EVT_ENC_SW` en mode PLAYER → `NEWMODE STATIONS` (sinon `player.toggle()`). En mode VOL, clic de `EVT_ENC_SW` → STATIONS directement.
2. `onBtnDoubleClick` : double-clic `EVT_ENC_SW` → `player.toggle()` (PLAYER ou VOL).
3. `Controls::init` : `rearmBtnPin()` (`gpio_reset_pin` + `pinMode`) pour chaque bouton, pour libérer GPIO43 de la fonction UART0. BACK marche depuis ce correctif (cause exacte non prouvée).
4. `controlsEvent`, branche STATIONS : anti-doublon 80 ms (`lastStep`) → 1 cran = 1 station (l'EC11 envoyait 2 événements par cran).

`src/displays/dspfont.h` : première branche `#if defined(PIGTEAM_LOGO) && DSP_HEIGHT >= 176` → `#include "bootlogo/pigteam.h"`.
`src/displays/bootlogo/pigteam.h` (nouveau) : logo PigTeam « Fairy Pig », **rond 128×128** RGB565, disque blanc / extérieur noir, `LOGO_TOP 12`. Généré depuis l'image 600×450 « The Pig Team Conception » (projet chrono).
`src/core/display.cpp` (`Display::_bootScreen`) : `#ifdef LOGO_TOP dsp.drawLogo(LOGO_TOP); #else dsp.drawLogo(BOOTLOGOTOP); #endif`.
`myoptions.h` : `#define PIGTEAM_LOGO` (bloc commun, avant `CLOCKFONT`) → logo actif sur tous les écrans ≥ 176 px de haut (Tiny, N16R8 ILI9341) ; les OLED 128×64 gardent le logo d'origine.

⚠️ **`ENC_STEPS` n'accepte que 1, 2 ou 4** (`#error` sinon, `options.h:581`). Valeur retenue : **4**. Ne pas mettre 8.

À refaire après une mise à jour d'ehRadio (trip5) : ces patchs écrasent `controls.cpp`, `dspfont.h` et `display.cpp` (le commit git permet de les retrouver).

## 🖥️ Réglages d'affichage

- **Vitesse de défilement** : `#define SCROLLTIME 30` dans `myoptions.h` (défaut 20 ms par pas = ~100 px/s sur 320×240 ; commun à tous les textes qui défilent). Retenu : réglé selon le goût de Noopy. Pour ralentir la météo seule : dernier argument de `.weatherConf` dans `src/displays/conf/displayTFT320x240conf.h` (`SCROLLTIME*2`).
- WebUI « Tampon audio » (`bufferbar`) : barre de remplissage du tampon audio (diagnostic des coupures). « Page volume » (`volumepage`) : écran dédié au volume quand on tourne le rotatif.
- Mise en page « VaraiTamas (BoomBox) » + horloge 7 segments (`CLOCKFONT YO_MONO`) : retenues (la mise en page est un réglage WebUI, pas dans `myoptions.h`).

## 🔋 Batterie (Lipo Rider Plus)

`BATTERY_PIN 1`, diviseur 10k/10k (×2). La section « Batterie » du WebUI est vide tant qu'aucune LiPo n'est branchée. Une fois branchée (⚠️ **vérifier la polarité du JST** avant) : démarrer sans USB, mesurer la tension aux bornes (mV), saisir la valeur dans « Lecture multimètre » et cliquer « étalonner ». Le pourcentage reste approximatif ; l'état de charge n'est pas lu (LED du Lipo Rider Plus). Pour masquer la section : `#define BATTERY_PIN 255`.

## 🧱 Pièges de compilation / flash (Tiny)

1. **Octal PSRAM** : `board_build.arduino.memory_type = qio_opi` (le N8R8 Tiny). Avec `qio_qspi` : `PSRAM: 0MB`, pas de serveur web, 4 Ko de heap libre. `qio_qspi` = uniquement la Zero 4 Mo (N4R2).
2. **Partitions** : `default_8MB.csv` (Tiny), `default_16MB.csv` (N16R8), `builds/partitions/4MBflash-SPIFFS.csv` (Zero). Toujours préciser `-e <env>` (un mauvais env → « does not fit in flash » ou `uploadfs` en échec).
3. **Lib SD** : Adafruit ST7789 dépend de `SD` → PlatformIO installe le SD 1.3.0 du registre (incompatible ESP32-S3). Ligne dans `lib_deps` : `SD=file://${platformio.packages_dir}/framework-arduinoespressif32/libraries/SD`, puis supprimer `.pio\libdeps\ehradio_s3_tiny_tft\SD`.
4. **ESPFileUpdater** : version corrigée dans `lib\ESPFileUpdater\` (pas dans `lib_deps`), voir doc de reprise n°7.
5. Nouvelle carte : `uploadfs` **d'abord**, puis `upload`.

## 🔊 Sortie audio

**PCM5102** (validé) : 3 ponts de soudure au dos : FLT, DEMP, FMT sur **L** (GND) ; XSMT (pont 3) sur **H** (3,3 V), jamais GND. Sur la Tiny : DIN 4, BCK 5, LCK 6 (déconnecter le GPS de 5/6 avant). Sortie niveau ligne (enceinte amplifiée ou casque).

**MAX98357 (DAC + ampli I2S mono, en attente de réception) — pour la version 100 % autonome** : mêmes fils I2S, **aucun changement de code** :

| MAX98357 | Tiny |
|---|---|
| DIN / BCLK / LRC | GPIO 4 / 5 / 6 |
| VIN | 5 V (sortie du Lipo Rider Plus) |
| GND | GND |
| SD | libre sur la plupart des cartes (mélange L+R en mono) — vérifier la sérigraphie des clones |
| GAIN | libre = 9 dB, GND = 12 dB, VIN = 6 dB |

Haut-parleur 4 à 8 Ω (~3 W sur 4 Ω en 5 V). Vérifier le courant max du 5 V du Lipo Rider Plus (ampli + écran + ESP32), commencer à volume modéré. Fils I2S courts, haut-parleur éloigné de l'écran, 100 µF près de VIN/GND si craquements. Peut rester en parallèle du PCM5102 pour comparer.

## 🔮 À faire

- [x] `git add/commit/push` (6 nov. : SD, logo, SCROLLTIME).
- [ ] Brancher et tester le **MAX98357** (version autonome : LiPo + Lipo Rider Plus + MAX98357 + haut-parleur).
- [ ] Brancher une LiPo et étalonner la batterie ; tester la luminosité (PWM GPIO 16).
- [ ] Tester la S3-Zero OLED (`-e ehradio_s3_zero`, `uploadfs` puis `upload`) et les 2 cartes douteuses.
- [ ] Envoyer l'issue à trip5 (`ISSUE_trip5_ESPFileUpdater.md`).
- [ ] Sculpture de circuit en laiton (Ø 1 mm commandé) : trouver l'idée de forme ; intégrer la radio dedans.
- [ ] Module autoradio (idée) : boîtier imprimé simple, voir doc de reprise (« Pistes »).
- [ ] Au besoin : logo PigTeam plus grand/plus petit (`pigteam.h` regénérable depuis l'image 600×450 du chrono).

*PIGTEAM 2026 🐷*
