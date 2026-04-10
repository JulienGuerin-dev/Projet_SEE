# Driver Ultrason HC-SR04 — STM32MP157C

Driver Linux embarqué pour le capteur ultrason HC-SR04 sur carte STM32MP157C, avec interface graphique temps réel.

---

## Sommaire

1. [Description du projet](#1-description-du-projet)
2. [Prérequis](#2-prérequis)
3. [Fichiers disponibles](#3-fichiers-disponibles)
4. [Créer et flasher l'image sdcard.img](#4-créer-et-flasher-limage-sdcardimg)
5. [Préparer la connexion SSH avec la carte](#5-préparer-la-connexion-ssh-avec-la-carte)
6. [Utiliser l'exécutable ultrason_control](#6-utiliser-lexécutable-ultrason_control)
7. [Interface graphique](#7-interface-graphique)

---

## 1. Description du projet

Ce projet est réalisé sur une carte **STM32MP157C** avec un capteur ultrason **HC-SR04**. L'objectif est de fournir un driver Linux embarqué permettant d'utiliser ce capteur via les GPIO de la carte, de traiter les signaux reçus pour calculer la distance d'un objet, et de proposer une interface utilisateur affichant en temps réel la distance mesurée ainsi que des informations statistiques : affichage d'une gaussienne, valeur moyenne et écart-type.

---

## 2. Prérequis

### Matériel

- Carte **STM32MP157C**
- Capteur ultrason **HC-SR04**
- Câble USB-C (alimentation)
- Câble Ethernet (connexion carte ↔ PC client)
- Carte SD (minimum 4 Go)

### Logiciels (PC client)

- Système d'exploitation **Linux**
- `nmap` — pour découvrir l'adresse IP de la carte sur le réseau
- `ssh` et `ssh-keygen` — pour la connexion sécurisée à la carte
- `python3` — pour lancer l'interface graphique
- Les bibliothèques Python suivantes :

```bash
pip install numpy scipy matplotlib
```

> `tkinter`, `subprocess`, `atexit`, `re` et `os` sont inclus dans la bibliothèque standard Python et ne nécessitent pas d'installation.

### Environnement de compilation (si recompilation de l'image)

- **Buildroot** installé et configuré sur le PC client

---

## 3. Fichiers disponibles

```
.
├── insa/
│   ├── 0001-add-hcsr04.patch         # Patch pour compléter le DTS (GPIO TRIG et ECHO)
│   └── config_finale           # Configuration Buildroot prête à compiler
├── ultrason/                   # Package Buildroot du driver (code source .c)
├── src/
│   └── ultrason_control.c      # Code source de l'exécutable de contrôle
│   └── distance.txt            # Fichier dans lequel les mesures sont enregistrées   
├── ultrason_control            # Exécutable compilé prêt à l'emploi
├── Makefile                    # Pour recompiler ultrason_control
└── affichage.py                # Interface graphique temps réel
```

---

## 4. Créer et flasher l'image sdcard.img

### 4.1 Recompiler l'image avec Buildroot

1. Se placer dans le répertoire Buildroot :

```bash
cd buildroot/
```

2. Copier le dossier `insa` dans `buildroot/board/` :

```bash
cp -r <chemin>/insa board/
```

3. Copier le dossier `ultrason` dans `buildroot/package/` :

```bash
cp -r <chemin>/ultrason package/
```

4. Charger la configuration finale et compiler :

```bash
make BR2_DEFCONFIG=./board/insa/config_finale defconfig
make
```

> La compilation peut prendre plusieurs dizaines de minutes selon votre machine.

L'image `sdcard.img` générée se trouvera dans `output/images/`.

### 4.2 Flasher l'image sur la carte SD

Identifier le nom de votre carte SD (par exemple `/dev/sdX`) avec la commande `dmesg -w`, puis flasher l'image :

```bash
sudo dd if=output/images/sdcard.img of=/dev/sdX bs=4M status=progress
sync
```

> **Attention** : remplacez `/dev/sdX` par le nom réel de votre carte SD. Une erreur à cette étape peut écraser un autre disque. Le plus courant est `/dev/sdb`.

---

## 5. Préparer la connexion SSH avec la carte

### 5.1 Connexion physique

Brancher le câble Ethernet entre la carte STM32MP157C et le PC client.

### 5.2 Trouver l'adresse IPv4 de la carte

```bash
nmap -sn <IPv4_PC>/<masque>
```

Exemple :

```bash
nmap -sn 10.42.0.1/24
```

Notez l'adresse IPv4 de la carte retournée par `nmap` (par exemple `10.42.0.15`).

### 5.3 Retourner dans le dossier du projet

```bash
cd Projet_SEE/
```

### 5.4 Configurer l'exécutable avec l'adresse IP de la carte

Ouvrir `src/ultrason_control.c` et modifier la ligne :

```c
#define IP4 "..."
```

en renseignant l'adresse IPv4 de la carte trouvée à l'étape précédente.

### 5.5 Créer une clé SSH

Pour éviter les demandes répétées de mot de passe lors des connexions SSH :

```bash
ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa_ultrason
```

### 5.6 Copier la clé publique sur la carte

```bash
ssh-copy-id -i ~/.ssh/id_rsa_ultrason.pub <user>@<IPv4>
```

Exemple :

```bash
ssh-copy-id -i ~/.ssh/id_rsa_ultrason.pub root@10.42.0.15
```

### 5.7 Recompiler l'exécutable ultrason_control

```bash
make
```

---

## 6. Utiliser l'exécutable ultrason_control

Toutes les commandes suivantes sont à exécuter depuis le terminal du PC client.

| Commande | Description |
|---|---|
| `./ultrason_control insmod` | Charge le driver ultrason sur la carte |
| `./ultrason_control mknod` | Crée le nœud `/dev/ultrason` sur la carte, relié au driver |
| `./ultrason_control remove` | Supprime le nœud `/dev/ultrason` et décharge le module |
| `./ultrason_control` | Lance une mesure unique. Le résultat est affiché dans le terminal et enregistré dans `distance.txt`. Le module est chargé et le nœud créé automatiquement si nécessaire |
| `./ultrason_control infinit` | Lance des mesures en continu toutes les 10 ms de manière infinie |

> Pour arrêter le mode `infinit`, utiliser `Ctrl+C`.

---

## 7. Interface graphique

L'interface graphique permet de visualiser en temps réel les mesures réalisées en mode `infinit`.

### 7.1 Vérifier l'implémentation du driver

Utiliser les commandes détaillées précédemment pour charger le module et créer le noeud si ce n'est pas déjà le cas

```bash
./ultrason_control insmod
./ultrason_control mknod
```

### 7.2 Lancer l'interface graphique

Lancer l'interface graphique :

```bash
python3 affichage.py
```

L'interface affiche en temps réel :
- la distance mesurée
- une courbe gaussienne
- la valeur moyenne et l'écart-type des mesures

---

## Auteurs

Antoine Gouezel, Dan Gruette, Ziad Tahti et Julien Guérin
