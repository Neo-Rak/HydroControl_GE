# HydroControl-GE Universal Firmware

Version: 3.0.0

## 1. Introduction

Ce document décrit le firmware universel pour le système d'irrigation intelligent **HydroControl-GE**. Ce firmware est conçu pour fonctionner sur un module ESP32 unique et peut être configuré pour endosser l'un des trois rôles distincts au sein de l'écosystème :

- **Centrale** : Le cerveau du système. Elle coordonne les actions des autres modules, fournit une interface utilisateur web pour la supervision, et gère les ressources partagées (comme les pompes de puits) pour éviter les conflits.
- **AquaReserv Pro** : Un module de contrôle de réservoir. Il surveille le niveau d'eau d'un réservoir et commande une pompe pour le remplir depuis un puits. Il peut fonctionner de manière autonome pour les ressources dédiées ou demander l'accès à une ressource partagée via la Centrale.
- **Wellguard Pro** : Un module actionneur pour une pompe de puits. Il reçoit des commandes directes (cryptées) pour activer ou désactiver la pompe.

L'architecture repose sur une communication sans fil robuste et sécurisée via LoRa, avec un cryptage AES-128 pour garantir la confidentialité et l'intégrité des commandes.

## 2. Architecture Logique

Le firmware est construit sur le framework Arduino avec FreeRTOS pour une gestion multitâche robuste, essentielle dans un contexte industriel.

### 2.1. Structure du Projet

Le projet est organisé de manière modulaire pour faciliter la maintenance et l'évolution :

- `src/main.cpp` : Le point d'entrée qui orchestre le chargement du rôle de l'appareil.
- `lib/` : Contient les bibliothèques locales, organisées par fonctionnalité :
    - `HGE_Crypto` : Gestion du cryptage/décryptage AES.
    - `HGE_Network` : Couche d'abstraction pour la communication LoRa et le provisionnement Wi-Fi.
    - `HGE_Roles` : Logique métier spécifique à chaque rôle (Centrale, AquaReserv, Wellguard).
    - `HGE_System` : Modules système de bas niveau, comme le `RoleManager` qui gère la persistance du rôle.
- `data/` : Contient les fichiers de l'interface web (HTML, CSS, JS) servis par l'ESP32.
- `platformio.ini` : Fichier de configuration du projet PlatformIO.

### 2.2. Communication

- **LoRa** : Utilisé pour la communication principale entre les modules en raison de sa longue portée et de sa faible consommation. Le protocole est défini dans `lib/HGE_Network/Message.h`.
- **Wi-Fi** : Utilisé uniquement lors de la phase de provisionnement initial. Un nouveau module démarre en mode point d'accès (AP) et sert une page web pour la configuration.
- **Sécurité** : Toutes les communications LoRa sont cryptées de bout en bout avec AES-128 et une clé pré-partagée.

## 3. Fonctionnalités Clés

- **Firmware Universel** : Un seul binaire pour tous les appareils, simplifiant le déploiement.
- **Provisionnement Simplifié** : Configuration initiale facile via une interface web sur le point d'accès Wi-Fi de l'appareil.
- **Haute Fiabilité** : Utilisation de FreeRTOS pour des opérations non bloquantes, et un mécanisme d'acquittement (ACK) pour les commandes critiques.
- **Sécurité Industrielle** : Cryptage AES-128 pour toutes les communications.
- **Arbitrage des Ressources** : La Centrale gère l'accès aux ressources partagées (pompes) pour éviter les conflits et les dommages matériels.
- **Interface de Supervision** : La Centrale offre un dashboard web pour visualiser l'état de l'ensemble du système en temps réel.

## 4. Diagnostic Visuel par LEDs

Tous les modules sont équipés d'un système de diagnostic visuel utilisant trois LEDs indépendantes (Rouge, Verte, Jaune) pour fournir un retour instantané sur l'état du système.

| Combinaison de LEDs                   | Signification                                      |
| ------------------------------------- | -------------------------------------------------- |
| **Jaune, clignotement lent**          | Démarrage en cours (Booting)                       |
| **Vert, fixe**                        | Système opérationnel, inactif (System OK)          |
| **Vert, clignotement lent**           | Action normale en cours (ex: pompe activée)        |
| **Jaune et Vert, alterné**            | Mode configuration (Setup Mode)                    |
| **Vert fixe + flash Jaune**           | Activité réseau (LoRa TX/RX)                       |
| **Jaune, fixe**                       | Avertissement (Warning), condition anormale        |
| **Rouge, clignotement rapide**        | Erreur Critique (Critical Error)                   |
| **Toutes éteintes**                   | Module non alimenté ou état OFF                    |

## 5. Brochage Matériel Universel

Pour simplifier la production et l'installation, tous les modules HydroControl-GE partagent **exactement le même câblage**. Le firmware active les broches nécessaires en fonction du rôle configuré.

### 5.1. Brochage Commun (pour tous les modules)

| Périphérique      | Broche ESP32 |
| ----------------- | ------------ |
| LoRa `SCK`        | `GPIO 18`    |
| LoRa `MISO`       | `GPIO 19`    |
| LoRa `MOSI`       | `GPIO 23`    |
| LoRa `NSS`        | `GPIO 5`     |
| LoRa `RST`        | `GPIO 14`    |
| LoRa `DIO0`       | `GPIO 2`     |
| LED Rouge         | `GPIO 15`    |
| LED Verte         | `GPIO 16`    |
| LED Jaune         | `GPIO 17`    |

### 5.2. Brochage Spécifique au Rôle

Les broches suivantes doivent être câblées en fonction du rôle le plus complexe que le module pourrait avoir à remplir.

| Broche      | Rôle : `AquaReserv Pro`        | Rôle : `Wellguard Pro`     | Rôle : `Centrale` |
| ----------- | ------------------------------ | -------------------------- | ----------------- |
| **`GPIO 25`** | Capteur Niveau Haut (Entrée)   | Commande Relais (Sortie)   | Non utilisé      |
| **`GPIO 26`** | Capteur Niveau Bas (Entrée)    | Défaut Matériel (Entrée)   | Non utilisé      |
| **`GPIO 27`** | Bouton Manuel (Entrée)         | Non utilisé                | Non utilisé      |

### 5.3. Instructions de Câblage

- **Pour `AquaReserv Pro`**:
  - Connectez les capteurs de niveau haut et bas (logique `INPUT_PULLUP`, l'autre fil à `GND`).
  - Connectez le bouton manuel (logique `INPUT_PULLUP`, l'autre fil à `GND`).
- **Pour `Wellguard Pro`**:
  - Connectez la commande du module relais.
  - Connectez le capteur de défaut externe (logique `INPUT_PULLUP`, l'autre fil à `GND`).
- **Pour la `Centrale`**:
  - Aucun câblage supplémentaire n'est requis.

## 6. Schémas de Câblage

```
ESP32 DevKit V1      Module LoRa (SX127x)
-----------------------------------------
GND ----------------- GND
3.3V ---------------- VCC
GPIO 18 ------------- SCK
GPIO 19 ------------- MISO
GPIO 23 ------------- MOSI
GPIO 5 -------------- NSS
GPIO 14 ------------- RST
GPIO 2 -------------- DIO0
```

### 5.2. Câblage Spécifique AquaReserv Pro

- **Capteurs à flotteur** : Connectez la broche de signal de chaque capteur à la GPIO correspondante. L'autre broche du capteur doit être connectée à la masse (GND). Le pull-up interne de l'ESP32 est utilisé.
- **Relais** : Connectez la broche de commande du module relais à la GPIO 27. Alimentez le module relais en 5V et GND depuis l'ESP32 (si le module le permet) ou une source externe.

### 5.3. Câblage Spécifique Wellguard Pro

- **Relais** : Connectez la broche de commande du module relais à la GPIO 27. Alimentez le module relais en 5V et GND.

## 6. Compilation et Téléversement

Le projet est conçu pour être compilé avec PlatformIO.

### 6.1. Prérequis

1. **Visual Studio Code** avec l'extension **PlatformIO IDE**.
2. Ou, la **PlatformIO Core (CLI)** installée (`pip install platformio`).

### 6.2. Procédure

1. **Clonez le projet** depuis le dépôt Git.
2. **Ouvrez le dossier `HydroControl_Universal`** dans Visual Studio Code ou dans votre terminal.
3. **Compilation** :
   - Dans VSCode : Cliquez sur l'icône PlatformIO dans la barre de gauche, puis sous "Project Tasks", choisissez "Build".
   - En CLI : Exécutez la commande `platformio run -d HydroControl_Universal/`
4. **Téléversement** :
   - Connectez votre module ESP32 en USB.
   - Dans VSCode : Cliquez sur "Upload".
   - En CLI : Exécutez la commande `platformio run --target upload -d HydroControl_Universal/`

### 6.3. Provisionnement d'un nouveau module

Après le premier téléversement, le module démarrera en mode "UNPROVISIONED".

1. Avec votre téléphone ou ordinateur, cherchez les réseaux Wi-Fi et connectez-vous au point d'accès **`HydroControl-Setup`**.
2. Une fois connecté, un portail captif devrait s'ouvrir. Sinon, ouvrez un navigateur et allez à l'adresse `http://192.168.4.1`.
3. Sur la page web, configurez :
   - Le **rôle** de l'appareil (Centrale, AquaReserv, ou Wellguard).
   - Les **identifiants Wi-Fi** de votre réseau local (SSID/Mot de passe).
   - La **clé de cryptage AES** (doit être la même pour tous les modules).
4. Sauvegardez la configuration. Le module redémarrera et adoptera son nouveau rôle.

---
_Documentation générée par Jules, Ingénieur Logiciel._
