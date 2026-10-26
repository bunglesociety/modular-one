# Modular One : compiler et installer sur le FM-1 (Mac)

Version bêta, testée uniquement sur le simulateur. Lis la partie « Avant d'installer » en entier.

## 1. Préparer le Mac (une seule fois)

1. **Docker Desktop** : installe-le et lance-le. Sur un Mac Apple Silicon, active dans ses
   réglages « Use Rosetta for x86_64/amd64 emulation » (la chaîne de compilation JieLi
   n'existe qu'en Linux x86-64, Docker la fait tourner).
2. **Python et Pillow**, dans le Terminal :
   ```
   pip3 install Pillow
   ```
3. **Le dossier du projet** : décompresse l'archive dans un dossier sous ton dossier
   utilisateur (par exemple `~/Desktop/modular-town-fm`), Docker doit pouvoir le lire.
4. **La chaîne de compilation JieLi** :
   ```
   cd ~/Desktop/modular-town-fm
   sh tools/get_toolchain.sh
   ```
5. **Le SDK JieLi AC79** (trois de ses fichiers entrent dans le paquet) :
   ```
   git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
   ```

## 2. Compiler

```
cd ~/Desktop/modular-town-fm
./build.sh
```

À la fin, le script affiche une liste de vérifications (`ok`) puis le fichier produit :
`build/felucca.fwsc` (le nom vient de Felucca, c'est bien Modular One dedans).
Si une ligne `FAIL` apparaît, ne flashe pas et envoie-moi la sortie du terminal.

Optionnel, les tests (dont les rendus de chaque preset dans `build/mt_demo/`) :
```
tests/run_tests.sh
```

## 3. Avant d'installer

- **Reviens d'abord au firmware officiel M-VAVE (V15 ou plus récent)**, avec M-UPGRADE ou
  l'installeur de Baud Girl. Felucca s'installe depuis le firmware officiel ; je n'ai pas pu
  vérifier le passage direct depuis FM-1+VA.
- Câble USB **directement** sur le Mac, pas de hub. Empêche le Mac de se mettre en veille.
- Si une installation échoue et que le FM-1 ne démarre plus : l'outil de récupération est
  [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## 4. Installer

Avec l'installeur web en local (Chrome ou Edge) :
```
python3 web/make_site.py build/felucca.fwsc dev /tmp/mt-site
cd /tmp/mt-site && python3 -m http.server 8000
```
Puis ouvre `http://localhost:8000/webapp/installer/` dans Chrome, branche le FM-1 et suis
les étapes.

Ou en ligne de commande :
```
pip3 install mido python-rtmidi
python3 tools/fm1_install.py --info
python3 tools/fm1_install.py build/felucca.fwsc
```

**Revenir en arrière** : le firmware officiel avec M-UPGRADE, ou FM-1+VA avec la page de Baud Girl.

## 5. Premier essai

1. Au démarrage, 5 secondes, à la manière des vieilles consoles : « BUNGLE SOCIETY » descend
   du haut de l'écran, s'arrête au milieu, le petit carillon sonne, puis « MODULAR ONE » (une note
   le passe).
2. Joue : la piste 1 est sur SOUFFLE, la 2 sur PRINTEMP, la 3 sur VELOURS EP. ALGORITHM change de piste.
3. PRESETS fait défiler les 40 presets ; sur la page PRESETS (bouton SAVE), le potard 1 choisit
   le son, le potard 2 passe d'une famille à l'autre.
4. Les potards de la page HOME : BRIL, MONT, SUS, REL. EDIT : page FM (BRIL, GRAIN, TIMB, CLOCH)
   puis page ESPACE (MONT, NUIT, ILE, SUB).

## Ce qui change par rapport au plugin

- L'enveloppe de volume est celle de Felucca (attaque linéaire, déclin et relâchement
  exponentiels, jusqu'à 10 s) avec la vélocité ; le sustain est libre.
- Le temps du delay suit le tempo (divisions), 0,557 s au plus à gauche.
- Delay et reverb sont des envois par piste (DLY, REV) vers des bus stéréo communs.
- La nuit n'allonge pas l'attaque.

## L'écran

Chaque page a son dessin. Un seul thème : l'écran vert des vieilles consoles portables, ses quatre
teintes et rien entre elles (les dégradés deviennent une trame fine), lettres pixel Silkscreen
(en grand au double de leur taille, comme les pixels de la ville), cadres carrés, jauges en blocs.
Le dessin montre ce que chaque potard bouge.
En haut : la piste, la page et ses sœurs (les petits points), la lecture, le tempo, la batterie.
En bas : les quatre valeurs.

- **HOME** : la famille, le nom du son, et l'onde qui sort, en direct.
- **FM** : les trois opérateurs (M2, M1 et sa boucle de feedback, la porteuse), leurs rapports,
  le désaccord, et un cycle de l'onde calculé comme le moteur.
- **ESPACE** : la montée, la lune (NUIT), l'île, le sub.
- **ENV**, **LFO**, **FX**, **DLY**, **REV/CHO**, **SCL**, **ARP**, **STEP**, **PATTERN**, **TRACKS** :
  la forme, les machines d'effet, le clavier et la gamme, le séquenceur, la table de mixage.
- Les autres pages : quatre faders, ou les choix empilés pour un réglage à options.

## Les 40 presets

Le son de Modular Town seulement, cinq familles de huit, pensées pour écrire de la musique de film :

- **SOUFFLES** (les nappes) : SOUFFLE, MAREE, ZEPHYR, PRINTEMP, VELOURS, NUIT, BRUINE, CREPUSCULE.
- **KEYS** (pianos électriques doux) : VELOURS EP, SALON, LAMPE, FEUTRE, VEILLEUSE, COTON, BOUGIE, DIMANCHE.
- **LUMIERES** (cloches et pincés feutrés) : CELESTA, BOITE, HARPE, VERRE, VIBRA, KALIMBA, GLOCKEN, CARILLON.
- **OMBRES** (graves et tension) : CONTRE, PULSE, ABYSSE, VIOLONC, CUIVRE, TREMOLO, RUMEUR, NOCTURNE.
- **NEON** (la ville la nuit, les synthés des films des années 80) : HORIZON, LARMES, BLUES, PLUIE,
  NEON, BRUME, PYRAMIDE, ECHO.

## Le MIDI (Studio One, etc.)

Le canal 1 joue la piste sélectionnée sur le FM-1, comme le clavier (et les canaux 4 à 16).
Le canal 2 joue toujours la piste 2, le 3 la piste 3.

## Le séquenceur

Trois pistes, les trois sons Modular Town ; la piste batterie de Felucca a été retirée (ses sons
aussi : environ 74 Ko de mémoire libérés). Chaque piste a son pattern de 16 à 64 pas (SEQ : STEP,
PATTERN), l'enregistrement en direct (REC), l'arpégiateur (ARP) ; TRACKS les montre côte à côte.
Les projets sauvegardés avant gardent leurs trois pistes de synthé ; leur piste batterie est ignorée.

## Le lecteur

OCT- et OCT+ maintenus ensemble 1 seconde : le lecteur démarre, sur le son de la piste choisie.
Il joue les morceaux envoyés depuis l'ordinateur, puis tes croquis, les uns après les autres,
et reprend au début après le dernier : il tourne en boucle jusqu'à ce que tu l'arrêtes.
L'écran montre un magnéto à bandes vu de dessus : les deux bobines tournent et la bande passe
de l'une à l'autre au fil du morceau. Pendant la lecture :

- **POTARD 1** : fait apparaître la liste des morceaux et des croquis ; tourne pour choisir, le
  morceau choisi démarre quand tu lâches le potard, et la liste disparaît 5 secondes après ;
- **OCT+** : morceau suivant ;
- **OCT-** : retour au début du morceau ; **deux fois** : morceau précédent ;
- **OCT- et OCT+ ensemble 1 seconde**, ou **HOME** : stop.
- **Effacer un croquis** : ouvre la liste avec le POTARD 1, place-toi sur le croquis et maintiens
  **REC** ; la ligne demande confirmation, maintiens **REC** encore une fois pour l'effacer (OCT- ou
  OCT+, ou tourner le potard, annule). Les croquis suivants gardent leur numéro, la lecture
  reprend au même endroit. Les morceaux envoyés depuis l'ordinateur s'effacent depuis l'ordinateur.

Pendant la lecture, OCT- et OCT+ ne changent plus l'octave du clavier ; tu peux jouer par-dessus,
et PRESETS change le son. Un premier morceau à essayer : `examples/01_satie_gymnopedie_1.mid`
(la 1re Gymnopédie d'Erik Satie, domaine public).

## Le carnet de croquis

Le FM-1 écoute en permanence ce que tu joues (clavier et MIDI), en mémoire seulement. Une idée
te plaît ? **Maintiens REC** (sur HOME, EDIT, ENV, etc. ; sur SEQ, ARP et TRACKS, REC maintenu
efface toujours la séquence) : il garde ta dernière phrase, depuis la première note après au moins
4 secondes de silence. L'écran affiche « CROQUIS 3  0:42 ». **Maintiens REC encore**, sans rien
jouer entre-temps : il remonte d'une phrase de plus. Les croquis rejoignent le lecteur et restent
après extinction (environ 20 croquis, selon leur longueur).

## Les morceaux et les croquis sur l'ordinateur

**L'app Carnet** (`web/carnet.html`) : ouvre-la dans **Chrome** (ou Edge), branche le FM-1 en USB,
clique **Connecter** et autorise le MIDI. À gauche les **morceaux** : dépose des fichiers .mid,
range-les en les glissant, renomme-les (12 lettres), écoute-les, efface-les avec ✕. Chaque
changement part tout seul sur le FM-1 (« ✓ Sur le FM-1 » quand c'est fait). À droite les **croquis** : écoute-les, exporte-les en .mid (un par un
ou tout en .zip), passe-les dans les morceaux avec la flèche, ou efface-les. Clique le nom d'un
croquis pour le renommer (12 lettres) : le nouveau nom apparaît aussi dans la liste du magnéto. Les morceaux du FM-1
s'exportent aussi en .mid. Pour l'essayer sans le FM-1 : `carnet.html?demo`.

Ou en ligne de commande :

```
pip3 install mido python-rtmidi
python3 tools/fm1_songs.py load mes_morceaux/        # jusqu'à 20 fichiers .mid (01_..., 02_...)
python3 tools/fm1_songs.py sketches mes_croquis/     # les croquis en fichiers .mid
python3 tools/fm1_songs.py info
python3 tools/fm1_songs.py clear-sketches            # vider le carnet
python3 tools/fm1_songs.py clear                     # plus de morceaux
```

Les morceaux et les croquis prennent deux des trois emplacements de samples de Felucca : il reste
l'emplacement USR1 pour tes samples.

## Le menu (HOME maintenu)

- **BOLD TEXT** : les petites lettres en gras, même typo, plus lisibles (activé par défaut).
- **LOW CUT** : coupe sous 110 Hz pour le petit haut-parleur (à éteindre au casque).
- **SCREEN SAVER** : après une minute sans toucher le synthé, la fenêtre sur Modular Town
  (la ville vue de l'autre côté de l'eau, qui suit le son), dans les mêmes quatre verts ; le
  premier geste la referme. Le potard 1 : à droite ON, à gauche OFF.
- **CALIBRATION**, **ABOUT**.

## Licence, à savoir avant de partager

Le code est sous GPL-3.0, comme Felucca : si tu distribues ce firmware, tu dois publier
son code source avec. Les sons de batterie de Felucca ne sont plus dans le firmware. Ses
icônes (`assets/icons.png`), qui n'étaient pas sous GPL, ont été retirées : le firmware se compile
sans icônes de paramètres, et peut donc être partagé sous GPL.
