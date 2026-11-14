# ⏰ Reveil par fichier audio (SD) — pigteam-radio

> Complement de `ECRAN_480x320_ILI9488.md` (section « Reveil »). Fait et valide le 14 novembre 2026.

## Principe
- Option de la page **Outils** (`/outils.html`, carte « Reveil ») : « Jouer un fichier audio du dossier **/reveil** (SD) ». Au declenchement, la radio **reste en mode web** : elle monte la SD, lit un fichier de `/reveil` avec la meme montee de volume (30 s, `ALARM_RAMP_MS`) puis demonte la SD a l'arret.
- **Fichiers acceptes** : mp3, m4a, aac, wav, flac, ogg, opus ; sous-dossiers de `/reveil` inclus (2 niveaux) ; fichiers caches (`.xxx`, `._xxx` de macOS) ignores.
- **Choix du fichier** : liste deroulante « Fichier » (« 🎲 Au hasard » par defaut) remplie par le bouton **🔍 Scanner /reveil**. Fichier choisi → joue **en boucle** jusqu'a l'arret ; au hasard → un autre fichier est tire a la fin de chacun.
- **Repli** : SD absente, dossier absent/vide ou fichier choisi disparu → tirage au hasard dans `/reveil`, puis, s'il n'y a rien, la **station** choisie (comme avant).
- **Le reveil ne joue que `/reveil`** (plus aucun repli sur le reste de la SD).
- **Le lecteur SD n'indexe pas `/reveil`** (`SDManager::_checkNoMedia` renvoie vrai pour ce dossier, donc ni liste ni comptage) : pas de doublons avec la musique. Apres la mise a jour, l'index SD se refait tout seul (le nombre de fichiers change) ; sinon forcer une re-indexation depuis le menu SD.

## Technique
- **Config** (`config.h/.cpp`) : `alarmfile` (bool, cle NVS `alarmfile`) et `alarmtrack` (chemin relatif a `/reveil`, ≤ 100 car., cle `alarmtrk`, vide = au hasard).
- **Commandes** (WebSocket / HTTP / MQTT / telnet) : `alarmfile=0/1`, `alarmtrack=<chemin>` (`-` ou vide = au hasard). Message WebSocket `ALARM` : ajout de `alarmfile` et `alarmtrack` (echappe pour le JSON).
- **HTTP** : `GET /alarmscan` → `{"count":N,"files":["a.mp3","sous/b.mp3"]}` ou `{"err":"..."}` (`handleAlarmScan` dans `netserver.cpp`, `Player::alarmScanJson`). Monte la SD le temps du scan (puis la demonte en mode web) ; **refuse** pendant un reveil MP3 ou une lecture SD (pas d'acces concurrent a la carte).
- **Lecture** (`player.cpp`) : `Player::_alarmPlayFile()` (tirage uniforme par echantillonnage, `connecttoFS(sdman, ...)` sans changer de mode), indicateur `_alarmFileMode`. **Fin de fichier** : `AudioHandlers::handleEofMp3` ne fait plus `player.next()` pendant le reveil, il demande a `Player::_alarmTick` de relancer un fichier. **Garde-fous** : la detection « flux mort » de `Player::loop()` est inactive pendant le mode fichier ; `_stop()` remet les indicateurs a zero et demonte la SD ; `_play()` (une station lancee pendant le reveil) sort du mode fichier.
- **Affichage** : nom de station « Reveil », titre = nom du fichier ; logs `[Alarm] MP3 : <fichier> (N fichiers)`, `Scan /reveil : N fichiers`.
- Fichiers touches : `player.cpp/.h`, `audiohandlers.cpp`, `sdmanager.cpp`, `config.h/.cpp`, `commandhandler.cpp`, `netserver.cpp`, `data/www/outils.html`. Pas de nouveau libelle de langue.

## Piege
- `outils.html` est dans `data/www` : changement → `uploadfs` puis « Restaurer les donnees » (Outils) ; le reste demande un reflash.

## Idees possibles (non faites)
- Liste de lecture plutot qu'un seul fichier en boucle ; ecoute d'essai du fichier depuis la page.
