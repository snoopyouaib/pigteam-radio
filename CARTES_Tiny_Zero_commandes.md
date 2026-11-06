# 🎛️ Cartes Tiny TFT / S3-Zero OLED — config et commandes (ehRadio)

> Complément de `REPRISE_PROJET_webradio.md` (qui décrit la N16R8). Dernière mise à jour : 6 novembre 2026.
> Dépôt : `D:\vscode_projets\ehradio-pigteam` (privé, GitHub `snoopyouaib/ehradio-pigteam`, remotes `origin` + `upstream`).

## 📦 État

| Carte | Env PlatformIO | État |
|---|---|---|
| ESP32-S3-Tiny N8R8 (8 Mo flash, PSRAM **octale** 8 Mo) + ST7789 320×240 | `ehradio_s3_tiny_tft` | ✅ démarre, Wi-Fi, WebUI, fr_FR, Europe/Paris, météo FR, **son PCM5102 OK**, commandes rotatif OK, pas de tactile (normal) |
| ESP32-S3-Zero 4 Mo (N4R2, PSRAM quad) + OLED SH1106 + rotatif + 2 boutons | `ehradio_s3_zero` | ⏳ config prête, pas encore testée (1 carte fonctionne, 2 peut-être abîmées par la chaleur) |
| ESP32-S3 N16R8 + ILI9341 / OLED | `ehradio_s3_n16r8` | ✅ voir doc de reprise |

Un seul `myoptions.h` pour les 3 : `-DCARTE_ZERO` et `-DCARTE_TINY_TFT` (flags de l'env) choisissent la branche ; sans flag = n16r8 (`ECRAN_OLED 0/1`). VS Code grise les branches inactives : c'est normal.

## 🔌 Tiny TFT — brochage

| Fonction | GPIO |
|---|---|
| ST7789 SCK / MOSI / MISO | 11 / 12 / 255 (pas de MISO) |
| TFT CS / DC / RST | 15 / 14 / 13 |
| Rétroéclairage (PWM, `DSP_DIMMING_ENABLED`) | 16 |
| Batterie (`BATTERY_DIVIDER_RATIO 2.0`) | 1 |
| Rotatif CLK / DT / SW | 17 / 18 / 44 |
| Bouton BACK (`BTN_PLAY`) | 43 |
| PCM5102 DIN / BCK / LCK | 4 / 5 / 6 |

GPIO 43/44 = UART0 TX/RX (d'où le re-armement des broches, voir ci-dessous). 0 et 3 = strapping, 19/20 = USB.

## 🎮 Commandes (sans tactile)

- **Tourner** : volume.
- **Clic rotatif** : ouvre la liste des stations ; dans la liste, tourner = choisir, clic = valider et sortir. (Marche aussi juste après un réglage de volume.)
- **Double-clic rotatif** : lecture / arrêt.
- **BACK (GPIO 43)** : lecture / arrêt (clic), appui long = bascule lecteur ↔ liste.
- `#define ENC_CLICK_OPENS_LIST` dans `myoptions.h` active ce schéma.

### Patchs dans `src/core/controls.cpp` (fichier complet dans le dépôt)

1. `onBtnClick` : sous `ENC_CLICK_OPENS_LIST`, clic de `EVT_ENC_SW` en mode PLAYER → `NEWMODE STATIONS` (sinon `player.toggle()`). En mode VOL, clic de `EVT_ENC_SW` → STATIONS directement.
2. `onBtnDoubleClick` : double-clic `EVT_ENC_SW` → `player.toggle()` (PLAYER ou VOL).
3. `Controls::init` : `rearmBtnPin()` (`gpio_reset_pin` + `pinMode`) pour chaque bouton, pour libérer GPIO43 de la fonction UART0. Hypothèse retenue pour BACK muet (non prouvée) ; le multimètre ne montrait aucun défaut matériel.
4. `controlsEvent`, branche STATIONS : anti-doublon 80 ms (`lastStep`) → 1 cran = 1 station (l'EC11 envoyait 2 événements par cran).

⚠️ **`ENC_STEPS` n'accepte que 1, 2 ou 4** (`#error` sinon, `options.h:581`). Valeur retenue : **4**. Ne pas mettre 8.

À refaire après une mise à jour d'ehRadio (trip5) : ces patchs écrasent `controls.cpp`.

## 🧱 Pièges de compilation / flash (Tiny)

1. **Octal PSRAM** : `board_build.arduino.memory_type = qio_opi` (le N8R8 Tiny). Avec `qio_qspi` : `PSRAM: 0MB`, pas de serveur web, 4 Ko de heap libre. `qio_qspi` = uniquement la Zero 4 Mo (N4R2).
2. **Partitions** : `default_8MB.csv` (Tiny), `default_16MB.csv` (N16R8), `builds/partitions/4MBflash-SPIFFS.csv` (Zero). Toujours préciser `-e <env>` (un mauvais env → « does not fit in flash » ou `uploadfs` en échec).
3. **Lib SD** : Adafruit ST7789 dépend de `SD` → PlatformIO installe le SD 1.3.0 du registre (incompatible ESP32-S3). Ligne dans `lib_deps` : `SD=file://${platformio.packages_dir}/framework-arduinoespressif32/libraries/SD`, puis supprimer `.pio\libdeps\ehradio_s3_tiny_tft\SD`.
4. **ESPFileUpdater** : version corrigée dans `lib\ESPFileUpdater\` (pas dans `lib_deps`), voir doc de reprise n°7.
5. Nouvelle carte : `uploadfs` **d'abord**, puis `upload`.

## 🔊 PCM5102 — rappel

3 ponts de soudure au dos : FLT, DEMP, FMT sur **L** (GND) ; XSMT (pont 3) sur **H** (3,3 V), jamais GND. Sur la Tiny : DIN 4, BCK 5, LCK 6 (déconnecter le GPS de 5/6 avant).

## 🔮 À faire

- [ ] `git add -f myoptions.h platformio.ini`, `git add -A`, commit, push (nouveaux `myoptions.h`, `platformio.ini`, `controls.cpp`). Optionnel : `git rm ehradio_fichiers_v12.zip`.
- [ ] Tester la Tiny : luminosité (PWM GPIO 16), lecture batterie (GPIO 1).
- [ ] Tester la S3-Zero OLED (`-e ehradio_s3_zero`, `uploadfs` puis `upload`) et les 2 cartes douteuses.
- [ ] Envoyer l'issue à trip5 (`ISSUE_trip5_ESPFileUpdater.md`).
- [ ] Mise en page « VaraiTamas (BoomBox) » + horloge 7 segments (`CLOCKFONT YO_MONO`) : retenues (réglage WebUI, pas dans `myoptions.h`).
- [ ] Sculpture de circuit en laiton (Ø 1 mm commandé) : trouver l'idée de forme ; intégrer la radio dedans (PCM5102 + enceinte amplifiée pour l'instant, posée sur le bureau/chevet).
- [ ] Module autoradio (idée) : boîtier imprimé simple, voir doc de reprise (« Pistes »).

*PIGTEAM 2026 🐷*
