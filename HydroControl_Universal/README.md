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

Tous les modules sont équipés d'un système de diagnostic visuel utilisant une LED RGB pour fournir un retour instantané sur l'état du système. Cela permet une maintenance et un dépannage rapides sur le terrain.

| Couleur & Motif                | Signification                                      | Rôles concernés |
| ------------------------------ | -------------------------------------------------- | --------------- |
| **Blanc, clignotement lent**   | Démarrage en cours (Booting)                       | Tous            |
| **Vert, fixe**                 | Système opérationnel, tout est normal (System OK)   | Tous            |
| **Bleu, fixe**                 | Mode configuration (Setup Mode / Provisioning)     | Tous            |
| **Cyan, clignotement**         | Transmission LoRa en cours                         | Tous            |
| **Magenta, clignotement**      | Réception LoRa en cours                            | Tous            |
| **Jaune, clignotement lent**   | Action en cours (ex: pompe activée)                | Tous            |
| **Orange, clignotement**       | Avertissement (Warning), condition anormale        | Tous            |
| **Rouge, clignotement rapide** | Erreur Critique (Critical Error), intervention requise | Tous            |

## 5. Configuration et Brochage (Pinout)

Chaque module nécessite un câblage spécifique pour ses périphériques. Les broches ont été choisies pour éviter les conflits avec les fonctions de base de l'ESP32.

**Note importante** : Utilisez une carte de développement ESP32 standard (type "ESP32 Dev Module").

### 5.1. Brochage Commun

- **Module LoRa (RFM95/SX127x)**:
  - `SCK`  -> `GPIO 18`
  - `MISO` -> `GPIO 19`
  - `MOSI` -> `GPIO 23`
  - `NSS`  -> `GPIO 5`
  - `RST`  -> `GPIO 14`
  - `DIO0` -> `GPIO 2`
- **LEDs de Statut (RGB)**:
  - `Rouge` -> `GPIO 15`
  - `Vert`  -> `GPIO 16`
  - `Bleu`  -> `GPIO 17`

### 4.2. AquaReserv Pro

- **Module LoRa**: Même brochage que la Centrale.
- **Capteur de Niveau Haut (flotteur)**:
  - `Signal` -> `GPIO 25` (configuré en INPUT_PULLUP)
- **Capteur de Niveau Bas (flotteur)**:
  - `Signal` -> `GPIO 26` (configuré en INPUT_PULLUP)
### 4.3. Wellguard Pro

- **Module LoRa**: Même brochage que la Centrale.
- **Commande Relais (pompe de puits)**:
  - `Signal` -> `GPIO 27`

## 5. Schémas de Câblage

(Cette section serait idéalement complétée avec des diagrammes, mais voici une description textuelle.)

### 5.1. Câblage Commun (Module LoRa)

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
