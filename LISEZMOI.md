# Modular One

**Un firmware pour la musique de film sur le M-VAVE FM-1**, par Alexis Arakis ([Bungle Society](https://bunglesociety.com)).

Modular One transforme le FM-1 en petit compagnon d'écriture : un moteur FM doux tiré du plugin
Modular Town, son delay, sa réverbe à plaque et son « île », une interface minimaliste dans les
quatre verts des consoles portables d'époque, une fenêtre sur Modular Town qui suit le son, et un
lecteur MIDI qui joue un morceau entier dans la machine.

Modular One est une version modifiée de [Felucca](https://github.com/hugelton/Felucca) de
Leo Kuroshita (Hügelton Instruments). Il n'est ni fait, ni approuvé, ni soutenu par
Hügelton Instruments ou par M-VAVE.

> **Bêta.** Il tourne sur le FM-1 de l'auteur et n'a pas encore été testé sur d'autres machines.
> Installer un firmware alternatif se fait à vos risques. Lisez « Avant d'installer » d'abord.

## Ce qu'il contient

- **Le moteur TOWN** : porteuse, modulateur avec feedback, second modulateur désaccordable,
  passe-bas doux. 40 presets en cinq familles, pensés pour écrire de la musique de film : SOUFFLES
  (nappes), KEYS (pianos électriques doux), LUMIERES (cloches feutrées), OMBRES (graves et tension),
  NEON (les synthés des films des années 80).
- **Les effets de Modular Town** : delay ping-pong stéréo, réverbe à plaque « années 80 », et l'île
  (un nuage scintillant sur tout le mix).
- **Les réglages** : BRIL (brillance), MONT (montée), NUIT (plus sombre, queues plus longues),
  ILE (l'île, on/off).
- **La fenêtre** : Modular Town de l'autre côté de l'eau ; l'heure, la lumière et l'eau suivent le
  son. Elle sert aussi d'écran de veille.
- **Le magnéto** : OCT- et OCT+ maintenus ensemble 1 seconde. Il joue tes morceaux (fichiers
  MIDI envoyés depuis l'ordinateur) et tes croquis, à la suite et en boucle, sur le son en cours, avec un magnéto à bandes
  à l'écran. Le POTARD 1 fait apparaître la liste pour choisir un morceau ou un croquis, OCT+ morceau suivant, OCT-
  retour au début, OCT- deux fois morceau précédent. On peut jouer par-dessus. Le même geste, ou
  HOME, l'arrête. Un premier morceau à essayer : `examples/01_satie_gymnopedie_1.mid` (Erik Satie,
  domaine public).
- **Le carnet de croquis** : le FM-1 écoute en permanence. Ce que tu viens de jouer te plaît ?
  Maintiens REC : il garde ta dernière phrase (depuis la première note après 4 s de silence).
  Maintiens REC encore : une phrase de plus en arrière. Les croquis restent après extinction et
  rejoignent le lecteur.
- **L'app Carnet** ([en ligne](https://bunglesociety.github.io/modular-one/carnet/), ou `web/carnet.html`, dans Chrome) : dépose des fichiers MIDI, range-les et
  renomme-les, envoie-les au FM-1 ; écoute tes croquis, exporte-les en MIDI, efface-les, fais-en des
  morceaux.
- **`tools/fm1_songs.py`** : la même chose en ligne de commande : envoie tes morceaux (`load dossier/`), récupère tes croquis en
  fichiers MIDI (`sketches dossier/`).
- Trois pistes de synthé. La piste batterie de Felucca a été retirée.

Tous les détails (pages, potards, notes techniques) : [MODULAR_TOWN.md](MODULAR_TOWN.md).

## Installer

**Le plus simple : [bunglesociety.github.io/modular-one](https://bunglesociety.github.io/modular-one/)**,
dans Chrome ou Edge sur un ordinateur. Branchez le FM-1 en USB, cliquez Installer, attendez environ
deux minutes. Le même site propose l'app **Carnet** pour les morceaux et les croquis.

Avant d'installer :

1. Branchez le câble USB **directement** sur l'ordinateur, sans hub. Empêchez la mise en veille.
2. Vos presets faits avec le firmware officiel sont perdus (on ne peut pas les exporter).
3. Si une installation échoue et que le FM-1 ne démarre plus, l'outil de récupération est
   [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

**En ligne de commande** à la place : téléchargez `modular-one-0.2-beta.fwsc` (page Releases, ou
`docs/firmware/` dans ce dépôt), puis :

```
pip3 install mido python-rtmidi
python3 tools/fm1_install.py --info
python3 tools/fm1_install.py modular-one-0.2-beta.fwsc
```

`--info` trouve seulement le FM-1 et n'écrit rien.

**Revenir en arrière** : réinstallez le firmware officiel avec l'outil M-UPGRADE de M-VAVE
(m-vave.com, rubrique Downloads).

SHA-256 de `modular-one-0.2-beta.fwsc` :
`1dac495c068e8a220598920bbcd79b03ce39d7d260966ae1b591ab6d54cc274c`

## Compiler depuis les sources

Comme Felucca : voir [BUILDING.md](BUILDING.md). Un guide en français pour Mac est dans
[docs/GUIDE_FR.md](docs/GUIDE_FR.md).

## Licence et crédits

- Code : **GPL-3.0-only**, comme Felucca (voir `LICENSE` et `LICENSING.md`). Le code source complet
  de ce firmware est dans ce dépôt.
- Felucca : Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments. Le README
  d'origine de Felucca est conservé dans [README_FELUCCA.md](README_FELUCCA.md).
- Modifications de Modular One : Copyright (C) 2026 Alexis Arakis, publiées sous GPL-3.0-only.
- Les éléments tiers gardent leur propre licence, listée dans `LICENSING.md`.
- « Felucca » et « Hügelton Instruments » sont des noms de Hügelton Instruments.
  « M-VAVE » et « FM-1 » sont des marques de leurs propriétaires respectifs.

English: [README.md](README.md)
