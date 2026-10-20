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
  passe-bas doux. 16 presets en deux familles : SOUFFLES (nappes) et KEYS (pianos électriques doux).
- **Les effets de Modular Town** : delay ping-pong stéréo, réverbe à plaque « années 80 », et l'île
  (un nuage scintillant sur tout le mix).
- **Les réglages** : BRIL (brillance), MONT (montée), NUIT (plus sombre, queues plus longues),
  ILE (l'île, on/off).
- **La fenêtre** : Modular Town de l'autre côté de l'eau ; l'heure, la lumière et l'eau suivent le
  son. Elle sert aussi d'écran de veille.
- **Le lecteur MIDI** : OCT- et OCT+ maintenus ensemble 1 seconde jouent la 1re Gymnopédie
  d'Erik Satie (domaine public, environ 3 minutes) sur la piste en cours. On peut jouer par-dessus. Le même geste, ou HOME, l'arrête.
- Trois pistes de synthé. La piste batterie de Felucca a été retirée.

Tous les détails (pages, potards, notes techniques) : [MODULAR_TOWN.md](MODULAR_TOWN.md).

## Avant d'installer

1. Revenez d'abord au **firmware officiel M-VAVE (V15 ou plus récent)** avec l'outil M-UPGRADE
   de M-VAVE (m-vave.com, rubrique Downloads).
2. Branchez le câble USB **directement** sur l'ordinateur, sans hub. Empêchez la mise en veille.
3. Si une installation échoue et que le FM-1 ne démarre plus, l'outil de récupération est
   [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Installer

Téléchargez `modular-one-0.1-beta.fwsc` sur la page Releases, puis, dans un terminal ouvert dans ce dépôt :

```
pip3 install mido python-rtmidi
python3 tools/fm1_install.py --info
python3 tools/fm1_install.py modular-one-0.1-beta.fwsc
```

`--info` trouve seulement le FM-1 et n'écrit rien.

**Revenir en arrière** : réinstallez le firmware officiel avec M-UPGRADE de M-VAVE.

SHA-256 de `modular-one-0.1-beta.fwsc` :
`beab4e0e28ff2ff3ef5073d6d63fa257c466c0fca25ba984f0f59335665184ca`

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
