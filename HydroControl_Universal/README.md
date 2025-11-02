# HydroControl-GE: Firmware Industriel Universel v3.1.0

## 1. Vue d'ensemble

Ce document détaille le firmware universel pour le système de gestion de l'eau **HydroControl-GE**. Conçu pour une fiabilité maximale dans des environnements exigeants, ce firmware unique s'installe sur tous les modules matériels ESP32 et se configure dynamiquement pour l'un des trois rôles critiques du système.

- **Centrale** : Le centre de commandement et de supervision. Elle orchestre les modules, gère l'accès aux ressources partagées (pompes de puits) pour éviter les conflits, et expose une interface web pour le monitoring en temps réel.
- **AquaReserv Pro** : Module de contrôle de réservoir. Il mesure le niveau d'eau, commande une pompe de remplissage, et gère un mode de fonctionnement manuel. Il communique avec la Centrale pour les ressources partagées ou directement avec un `Wellguard Pro` pour les ressources dédiées.
- **Wellguard Pro** : Module actionneur sécurisé pour une pompe de puits. Il exécute les commandes de pompage reçues via le réseau LoRa crypté.

L'épine dorsale du système est une communication sans fil LoRa longue portée, sécurisée par un cryptage AES-128 de bout en bout.

## 2. Architecture et Philosophie de Conception

Le firmware est construit sur le **framework Arduino avec FreeRTOS**, ce qui permet une gestion multitâche préemptive, essentielle pour garantir la réactivité et la fiabilité du système.

### 2.1. Principes Clés

- **Firmware Unique** : Un seul binaire pour tous les appareils simplifie radicalement la fabrication, le déploiement et la maintenance.
- **Robustesse** : Chaque opération critique (communication, lecture de capteur) est gérée dans une tâche FreeRTOS dédiée pour éviter tout blocage. Un watchdog matériel prévient les blocages système.
- **Sécurité** : Les communications LoRa sont cryptées avec AES-128 et une clé pré-partagée de 16 octets, empêchant toute commande non autorisée.
- **Découverte Automatique** : Les modules se découvrent automatiquement sur le réseau, simplifiant l'ajout de nouveaux appareils.

### 2.2. Structure du Projet (v3.1.0)

Le projet est organisé pour une modularité et une maintenabilité maximales :

- `src/main.cpp` : Point d'entrée qui lit le rôle configuré et lance la logique appropriée.
- `lib/` : Contient les bibliothèques logicielles internes :
    - `HGE_Comm` : Code de communication commun (format des messages, cryptage).
    - `HGE_Central` : Logique métier spécifique à la `Centrale`.
    - `HGE_Roles` : Logique pour les modules de terrain (`AquaReservLogic`, `WellguardLogic`).
    - `HGE_System` : Modules système transversaux (gestion des LEDs, du watchdog, du rôle).
- `data/` : Fichiers de l'interface web (HTML, CSS, JS) servis par la `Centrale` ou pour le provisionnement.
- `platformio.ini` : Fichier de configuration PlatformIO qui gère les dépendances et les paramètres de compilation.

## 3. Brochage Matériel Universel et Câblage

Pour standardiser la production, tous les modules HydroControl-GE partagent **le même brochage physique**. Le firmware active uniquement les broches nécessaires au rôle configuré.

### 3.1. Brochage Commun (pour tous les modules)

| Périphérique      | Broche ESP32 |
| ----------------- | ------------ |
| LoRa `SCK`        | `GPIO 18`    |
| LoRa `MISO`       | `GPIO 19`    |
| LoRa `MOSI`       | `GPIO 23`    |
| LoRa `NSS` / `CS` | `GPIO 5`     |
| LoRa `RST`        | `GPIO 14`    |
| LoRa `DIO0`       | `GPIO 2`     |
| LED Rouge         | `GPIO 15`    |
| LED Verte         | `GPIO 16`    |
| LED Jaune         | `GPIO 17`    |

### 3.2. Brochage Spécifique au Rôle ("ROLE_PIN")

Ces broches sont câblées en fonction du rôle le plus complexe (ici, `AquaReserv Pro`).

| Broche      | Rôle : `AquaReserv Pro`        | Rôle : `Wellguard Pro`     | Rôle : `Centrale` |
| ----------- | ------------------------------ | -------------------------- | ----------------- |
| **`GPIO 25`** | Capteur Niveau Haut (Entrée)   | Non utilisé                | Non utilisé      |
| **`GPIO 26`** | Capteur Niveau Bas (Entrée)    | Commande Relais (Sortie)   | Non utilisé      |
| **`GPIO 27`** | Bouton Manuel (Entrée)         | Non utilisé                | Non utilisé      |

### 3.3. Schémas de Câblage Détaillés

**Module LoRa (SX127x) vers ESP32 DevKit V1:**
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

**Instructions de câblage pour `AquaReserv Pro`:**
- **Capteurs à flotteur** : Connectez une borne de chaque capteur à `GND`. Connectez l'autre borne aux GPIOs correspondantes (`GPIO 25` pour le niveau haut, `GPIO 26` pour le niveau bas). Le firmware utilise les résistances de PULL-UP internes.
- **Bouton Manuel** : Connectez une borne du bouton poussoir à `GND` et l'autre à `GPIO 27`.

**Instructions de câblage pour `Wellguard Pro`:**
- **Module Relais** : Connectez la broche de commande (`IN`) du module relais à `GPIO 26`. Alimentez le module relais (`VCC`, `GND`) depuis une source 5V appropriée.

## 4. Diagnostic Visuel par LEDs

Le système de LEDs fournit un état instantané du module.

| Combinaison de LEDs                   | Signification                                      |
| ------------------------------------- | -------------------------------------------------- |
| **Jaune, clignotement lent**          | Démarrage en cours (Booting)                       |
| **Vert, fixe**                        | Système OK, en attente.                            |
| **Vert, clignotement lent**           | Action en cours (ex: pompe activée).               |
| **Jaune et Vert, alterné**            | Mode Provisionnement / Configuration.              |
| **Flash Jaune rapide**                | Activité LoRa (Réception/Transmission).            |
| **Jaune, fixe**                       | Avertissement (ex: capteur en état incohérent).    |
| **Rouge, clignotement rapide**        | Erreur Critique (ex: LoRa non détecté).            |

## 5. Installation et Déploiement

### 5.1. Prérequis

1.  **Visual Studio Code** avec l'extension **PlatformIO IDE**.
2.  Ou, la **PlatformIO Core (CLI)** : `pip install platformio`.

### 5.2. Compilation et Téléversement

1.  Clonez ce dépôt.
2.  Ouvrez le dossier `HydroControl_Universal` dans VS Code ou votre terminal.
3.  **Compilation** : `platformio run -d HydroControl_Universal/`
4.  **Téléversement** : Connectez l'ESP32 et exécutez `platformio run --target upload -d HydroControl_Universal/`

### 5.3. Provisionnement d'un Nouveau Module

Tout nouveau module doit être configuré :

1.  À la première mise sous tension, le module crée un point d'accès Wi-Fi nommé **`HydroControl-Setup`**.
2.  Connectez-vous à ce réseau avec un téléphone ou un ordinateur.
3.  Ouvrez un navigateur et allez à l'adresse `http://192.168.4.1`.
4.  Depuis l'interface web, configurez :
    -   Le **Rôle** de l'appareil (`Centrale`, `AquaReserv Pro`, `Wellguard Pro`).
    -   Les **identifiants de votre réseau Wi-Fi** (SSID & mot de passe) - *Nécessaire uniquement pour la Centrale*.
    -   La **Clé de Cryptage LoRa (PSK)** : une chaîne de **16 caractères exactement**, qui doit être identique sur tous les modules du réseau.
5.  Sauvegardez. Le module redémarre dans son rôle opérationnel.

## 6. Dépannage (Troubleshooting)

- **Un module n'apparaît pas sur l'interface de la Centrale** :
    1.  Vérifiez que la clé de cryptage LoRa (PSK) est identique sur la Centrale et le module.
    2.  Vérifiez le câblage du module LoRa.
    3.  Assurez-vous que l'antenne est correctement connectée.
    4.  Observez les LEDs : un flash jaune indique une communication. S'il n'y en a jamais, le problème est probablement matériel.

- **Le module redémarre en boucle** :
    1.  Ceci est souvent causé par le watchdog. Cela signifie qu'une tâche est bloquée.
    2.  Connectez le module à un ordinateur et ouvrez le moniteur série (`platformio device monitor`) pour voir les messages d'erreur.
    3.  Cela peut être dû à une alimentation électrique instable. Assurez-vous d'utiliser une alimentation de qualité.

- **Impossible de se connecter au Wi-Fi de provisionnement** :
    1.  Rapprochez-vous du module.
    2.  Essayez de "Oublier" le réseau sur votre appareil et de vous reconnecter.
    3.  Redémarrez le module HydroControl.

---
_Documentation Technique v3.1.0 - Maintenue par Jules, Ingénieur Logiciel._
