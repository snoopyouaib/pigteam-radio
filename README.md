# pigteam-radio

Web radio ESP32-S3 de la **PigTeam**, basée sur [ehRadio](https://github.com/trip5/ehRadio) (trip5),
elle-même dérivée de [yoRadio](https://github.com/e2002/yoradio) (e2002).
Le projet est désormais maintenu de façon indépendante : il ne suit plus les mises à jour d'ehRadio.
Voir [NOTICE](NOTICE) pour les crédits et la licence.

## Matériel de référence (environnement `ehradio_s3_n16r8`)

| Élément | Détail |
|---|---|
| Carte | ESP32-S3 DevKitC-1 N16R8 (16 Mo flash, 8 Mo PSRAM octale) |
| Écran | TFT 4" 480x320 ILI9488 (SPI) |
| Tactile | XPT2046 (bus SPI partagé avec l'écran) |
| Carte SD | Lecteur de l'écran, sur un bus SPI séparé |
| Son | DAC I2S PCM5102 (ou ampli MAX98357, mêmes broches DIN/BCLK/LRC) |

### Câblage

| Fonction | GPIO |
|---|---|
| SCK écran + tactile (SPI A) | 12 |
| MOSI écran + tactile | 11 |
| MISO tactile (T_DO) | 13 |
| TFT_CS | 10 |
| TFT_DC | 9 |
| TFT_RST | 3 |
| T_CS (tactile) | 14 |
| Rétroéclairage (broche LED, PWM) | 4 |
| SD SCK / MOSI / MISO (SPI B) | 48 / 47 / 21 |
| SD CS | 2 |
| I2S DIN / BCLK / LRC | 42 / 41 / 40 |

Notes :
- ILI9488 : la broche SDO (MISO de l'écran) reste **non connectée** ; seul T_DO du tactile va sur le GPIO 13.
- Le module 4" ne relie pas entre eux les SCK/MOSI de l'écran et du tactile : prévoir un point de soudure commun par signal.
- GPIO à éviter sur N16R8 : 26 à 37 (flash/PSRAM), 19/20 (USB), 0/3/45/46 (strapping, sauf 3 avec la configuration par défaut).

## Fonctions ajoutées ou modifiées par rapport à ehRadio

- Support de l'écran ILI9488 + XPT2046 avec inversion écran/tactile (`SCREEN_FLIP`, `TOUCH_FLIP`).
- Tactile : appui long immobile = bascule radio <-> carte SD (`TS_LONGPRESS_MS`), seuil de balayage `TS_SWIPE_THRESHOLD_PX`.
- Correction d'une course dans les balayages verticaux (volume au lieu de liste de stations).
- Rétroéclairage réglable (`BRIGHTNESS_PIN`, `DSP_DIMMING_ENABLED`).
- Logo de démarrage PigTeam, police d'horloge et valeurs par défaut locales (`myoptions.h`).
- Clics Radio-Browser désactivés (`RADIO_BROWSER_NO_SEND_CLICKS`).

## Compilation

Prérequis : [PlatformIO](https://platformio.org/) (VS Code).

```
pio run -e ehradio_s3_n16r8          # firmware
pio run -e ehradio_s3_n16r8 -t upload
pio run -e ehradio_s3_n16r8 -t uploadfs   # interface web (dossier data/)
```

La configuration matérielle se règle dans `myoptions.h` (choix de l'écran avec `ECRAN`) et `platformio.ini`.

Au premier démarrage après un flashage, ehRadio reste en mode sécurisé (Smartstart et mode SD
désactivés) jusqu'à 30 s de fonctionnement stable : lancer la lecture manuellement.

## Licence

GNU GPL v3 — voir [LICENSE](LICENSE).
