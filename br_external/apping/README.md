# Apping

Application C++/Qt pour piloter des bases fixes de diffusion audio sur un LAN sans serveur central.

Le dépôt contient maintenant :

- `portable-console` : interface `Qt Widgets` en français, compacte type mobile, avec vraie carte offline interactive du secteur Télécom Physique Strasbourg, sélection de bases, déclenchement des messages préenregistrés, mégaphone live et enregistrement différé, modifiable dans Qt Creator via `.ui`.
- `fixed-base-service` : service headless par poteau/base, découverte UDP, API HTTP locale, bibliothèque audio, lecture locale et réception du live via `roc_recv`.
- `sim-launcher` : lancement d'une simulation locale multi-bases sur un seul PC.
- `roc-toolkit-opus-master` : fork ROC existant, utilisé tel quel pour le transport live en Opus.

## Dépendances

Prévoir au minimum :

- `cmake >= 3.21`
- `Qt 6.5+` avec `Core`, `Gui`, `Network`, `Multimedia`, `Qml`, `Quick`
- une version construite de `roc_send` et `roc_recv` disponible soit dans `PATH`, soit via les variables d'environnement `ROC_SEND` / `ROC_RECV`

Le projet applicatif n'essaie pas de recompiler ROC automatiquement.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Lancement rapide en simulation

1. Construire `roc_send` / `roc_recv` dans votre fork ROC.
2. Construire les binaires Qt.
3. Lancer :

```bash
./scripts/run-local-sim.sh
```

Cela démarre :

- `fixed-base-service` avec des configs runtime générées dans `config/simulation/generated/`
- `portable-console --portable-config config/simulation/generated/portable-console.json`

Les scripts de simulation :

- utilisent d'abord l'audio de session (`pipewire-pulse` / PulseAudio utilisateur) quand il est disponible
- démarrent un runtime PulseAudio privé seulement en repli
- sélectionnent un vrai sink/source PulseAudio quand il y en a un
- basculent sur `apping_null` seulement si aucune vraie sortie n'est détectée
- forcent `QT_QPA_PLATFORM=xcb`, `wayland` ou `offscreen` selon l'environnement disponible

Si vous voulez juste une base et la console :

```bash
./scripts/run-alpha-console.sh
```

Si la sortie affiche `Qt platform: offscreen`, l'application tourne mais il n'y a pas de session graphique exploitable sur ce shell. Dans ce cas, lancer le script depuis une session bureau X11/Wayland valide pour voir l'interface.

## Qt Creator

Le point d'entrée du projet est :

```bash
CMakeLists.txt
```

Le dépôt inclut maintenant :

- `CMakePresets.json` avec `qtcreator-debug` et `qtcreator-release`
- `scripts/open-qtcreator.sh` pour préparer l'environnement local puis ouvrir Qt Creator
- des configs runtime générées dans `config/simulation/generated/` pour lancer `portable-console`, `fixed-base-service` et `sim-launcher` sans arguments supplémentaires
- une interface Designer dans `src/portable_console/mainwindow.ui`
- une vraie carte offline dans `src/portable_console/qml/LiveMap.qml`

Commande recommandée :

```bash
./scripts/open-qtcreator.sh
```

## Remarques d'exploitation

- Les bases annoncent leur présence par `UDP broadcast` sur le port `17100`.
- Les commandes passent en HTTP JSON local sur les ports `18101+`.
- Le live mégaphone utilise ROC en RTP Opus mono sur les ports `19101+`.
- Les nouveaux messages enregistrés depuis le portable sont encodés en WAV PCM et stockés localement sur chaque base ciblée.

## Arborescence utile

- `assets/map/` : métadonnées et tuiles offline pour la simulation Télécom Physique Strasbourg
- `config/simulation/` : configs prêtes pour un test local
- `config/simulation/runtime/*/library/` : bibliothèques locales simulées des bases
- `src/common/` : modèles, protocole JSON, helper ROC, HTTP minimal, WAV
- `src/fixed_base_service/` : service des bases fixes
- `src/portable_console/` : UI opérateur `Qt Widgets/.ui` et logique portable

Note carte :

- la carte utilise le plugin OSM de Qt Location avec un serveur HTTP local embarqué
- les tuiles de la simulation sont stockées dans `assets/map/telecom_physique_strasbourg_tiles`
- le script `scripts/fetch_offline_osm_tiles.py` permet de régénérer un paquet de tuiles offline
