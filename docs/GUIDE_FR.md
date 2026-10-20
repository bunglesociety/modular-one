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
3. PRESETS fait défiler les 16 presets ; sur la page PRESETS (bouton SAVE), le potard 1 choisit
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

## Les 16 presets

Le son de Modular Town seulement, deux familles de huit :

- **SOUFFLES** : SOUFFLE, MAREE, ZEPHYR, PRINTEMP, VELOURS, NUIT, BRUINE, CREPUSCULE.
- **KEYS** : des pianos électriques à lames, doux : VELOURS EP (BRIL 20 %), SALON, LAMPE,
  FEUTRE, VEILLEUSE, COTON, BOUGIE, DIMANCHE.

## Le MIDI (Studio One, etc.)

Le canal 1 joue la piste sélectionnée sur le FM-1, comme le clavier (et les canaux 4 à 16).
Le canal 2 joue toujours la piste 2, le 3 la piste 3.

## Le séquenceur

Trois pistes, les trois sons Modular Town ; la piste batterie de Felucca a été retirée (ses sons
aussi : environ 74 Ko de mémoire libérés). Chaque piste a son pattern de 16 à 64 pas (SEQ : STEP,
PATTERN), l'enregistrement en direct (REC), l'arpégiateur (ARP) ; TRACKS les montre côte à côte.
Les projets sauvegardés avant gardent leurs trois pistes de synthé ; leur piste batterie est ignorée.

## La démo

OCT- et OCT+ maintenus ensemble 1 seconde : le synthé joue la 1re Gymnopédie d'Erik Satie
(domaine public, environ 3 min 15), sur la piste choisie avec un son à lui, doux et chaud ; le son de la piste revient à la fin. La fenêtre
sur la ville s'ouvre pendant que ça joue ; tu peux jouer par-dessus au clavier. Le même geste, ou
HOME, l'arrête.

## Le menu (HOME maintenu)

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
