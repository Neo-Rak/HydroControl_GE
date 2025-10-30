### **Fiche Technique du Système — HydroControl-GE**

**Référence Document :** HGE-TS-232-FR
**Version :** 1.0
**Applicable au Firmware :** v2.3.2 et ultérieures
**Date d'Émission :** 29 octobre 2025

#### **1.0 Description Générale et Architecture Système**

Le système HydroControl-GE est une solution IoT industrielle de bout-en-bout conçue pour la supervision et l'automatisation de la gestion hydraulique. L'architecture est basée sur une topologie réseau en étoile coordonnée, avec des capacités de communication directe redondante entre les nœuds périphériques. Le système se compose de trois classes de dispositifs spécialisés interagissant au sein d'un réseau de communication radio sécurisé.

*   **Coordinateur de Réseau (`Centrale HydroControl-GE`) :** Agit comme passerelle réseau, orchestrateur de la topologie, serveur d'application embarqué et point de terminaison pour la collecte et la persistance des données d'état.
*   **Nœud de Contrôle (`AquaReservPro`) :** Unité de traitement autonome qui exécute la logique de contrôle décentralisée basée sur l'acquisition de données de capteurs locaux.
*   **Nœud Actionneur (`WellguardPro`) :** Unité terminale d'exécution, recevant des directives de commande sécurisées pour l'actionnement de relais de puissance.

#### **2.0 Spécifications du Processeur Central (Unité de Traitement des Modules)**

Chaque module du système est architecturé autour d'une unité de traitement de haute performance présentant les caractéristiques suivantes :

*   **Architecture :** Microprocesseur 32-bit à double cœur de type Xtensa® LX6.
*   **Fréquence d'Horloge :** Dynamiquement ajustable, jusqu'à 240 MHz par cœur.
*   **Mémoire Statique (SRAM) :** 520 Ko intégrés.
*   **Mémoire Programme (Flash) :** 4 Mo de mémoire Flash non volatile.
*   **Co-processeur ULP :** Un co-processeur à ultra-basse consommation permettant des opérations de veille profonde et de réveil sur interruption.
*   **Périphériques Intégrés :**
    *   Contrôleurs d'accès direct à la mémoire (DMA).
    *   Convertisseurs Analogique-Numérique (ADC) 12-bit.
    *   Interfaces série synchrones et asynchrones : SPI, I2C, UART.
    *   Générateurs de signaux PWM (Modulation de largeur d'impulsion).
*   **Jeu d'Instructions :** ISA (Instruction Set Architecture) 32-bit optimisé pour les applications embarquées basse consommation.

#### **3.0 Spécifications Techniques par Module**

| Caractéristique | Centrale HydroControl-GE | Module AquaReservPro | Module WellguardPro |
| :--- | :--- | :--- | :--- |
| **Rôle Principal** | Coordinateur / Serveur Web | Contrôle de Niveau / Logique | Commande de Pompe |
| **Unité de Traitement** | Dual-Core 32-bit @ 240 MHz | Dual-Core 32-bit @ 240 MHz | Dual-Core 32-bit @ 240 MHz |
| **Transceiver Principal** | Semtech SX1278 (LoRa) | Semtech SX1278 (LoRa) | Semtech SX1278 (LoRa) |
| **Connectivité Secondaire**| Wi-Fi 802.11 b/g/n (2.4 GHz) | - | - |
| **Interface de Puissance** | USB Type-C (5V DC) | USB Type-C (5V DC) | USB Type-C (5V DC) |
| **Indicateurs Visuels** | 3x LED (Rouge, Jaune, Bleu) | 3x LED (Rouge, Jaune, Bleu) | 3x LED (Rouge, Jaune, Bleu) |
| **Interface Opérateur** | Serveur Web Async HTTP/SSE | Bouton-poussoir (GPIO) | Bouton-poussoir (GPIO) |
| **Entrées Capteur** | - | 1x Entrée Numérique (GPIO) | - |
| **Sorties Actionneur** | - | - | 1x Sortie Numérique (GPIO) |

#### **4.0 Pile de Protocoles de Communication**

##### **4.1 Couche Physique (PHY) - LoRa**

*   **Module Transceiver :** Basé sur le chipset Semtech SX1278.
*   **Bande de Fréquence :** ISM 433 MHz (433.05 - 434.79 MHz).
*   **Technique de Modulation :** CSS (Chirp Spread Spectrum) brevetée par Semtech.
*   **Puissance d'Émission (Tx) :** Configurable, jusqu'à +20 dBm (100 mW).
*   **Sensibilité de Réception (Rx) :** Jusqu'à -139 dBm.
*   **Bande Passante (BW) :** Configurable (typiquement 125 kHz).
*   **Facteur d'Étalement (SF) :** Configurable (typiquement SF7 à SF12).

##### **4.2 Couche de Liaison de Données (MAC) - Protocole HydroControl**

*   **Identification des Nœuds :** L'adresse MAC 48-bit de l'interface Wi-Fi est utilisée comme identifiant unique et inaltérable pour chaque nœud.
*   **Structure de Paquet :** Les données sont encapsulées dans une trame JSON contenant des métadonnées en clair (type, source, destination) et une charge utile chiffrée.
*   **Mécanisme de Fiabilité :**
    *   **Acquittement (ACK) :** Toutes les directives de commande (`CMD_PUMP`) requièrent un acquittement de la part du nœud actionneur.
    *   **Politique de Réessai :** En cas d'échec de réception de l'ACK, la commande est réémise jusqu'à 2 fois (total de 3 tentatives).
    *   **Timeout d'ACK :** 2000 millisecondes.
*   **Mécanisme de Supervision :**
    *   **Heartbeat :** Chaque nœud émet un paquet de statut périodique (`STATUS_UPDATE`) pour signaler sa présence et son état.
    *   **Intervalle de Heartbeat :** 120 000 millisecondes (2 minutes). Le heartbeat est omis si une communication critique a eu lieu pendant cet intervalle.
    *   **Timeout de Déconnexion :** Un nœud est déclaré `DISCONNECTED` par la Centrale après 300 000 millisecondes (5 minutes) d'inactivité.

##### **4.3 Couche de Présentation et Session - Sécurité**

*   **Authentification :** Une clé pré-partagée (Pre-Shared Key - PSK) est requise pour l'enregistrement initial d'un nœud sur le réseau.
*   **Chiffrement :** La charge utile (`payload`) de chaque paquet est chiffrée via l'algorithme **AES-128 en mode CBC** (Cipher Block Chaining).
*   **Encapsulation :** Le `payload` binaire chiffré est encodé en **Base64** pour une transmission textuelle sécurisée au sein de la trame JSON.

##### **4.4 Couche Application - Protocole HydroControl**

*   **Format des Données :** Objets JSON (JavaScript Object Notation).
*   **Types de Messages Définis :** `DISCOVERY`, `CMD_PUMP`, `ACK_PUMP`, `STATUS_UPDATE`, `CONFIG_ASSIGN`, etc.
*   **Serveur d'Application (Centrale) :** Serveur web asynchrone sur TCP/IP, utilisant HTTP/1.1 pour les requêtes API et Server-Sent Events (SSE) pour la diffusion des mises à jour en temps réel au tableau de bord.

#### **5.0 Spécifications de la Connectivité Wi-Fi (Centrale Uniquement)**

*   **Standard :** IEEE 802.11 b/g/n.
*   **Bande de Fréquence :** 2.4 GHz.
*   **Modes de Fonctionnement :** Station (STA) et Point d'Accès (AP) pour la configuration initiale.
*   **Sécurité :** Support WPA2/WPA3-Personal (PSK).

#### **6.0 Pile Logicielle et Firmware**

*   **Système d'Exploitation :** Système d'exploitation temps réel (RTOS) préemptif basé sur le noyau FreeRTOS.
*   **Architecture des Tâches :** Multitâche, avec des tâches dédiées pour la gestion des communications, la logique de contrôle, le serveur web, le diagnostic par LED et la supervision des nœuds.
*   **Framework de Développement :** Construit sur le framework Arduino Core avec des optimisations spécifiques pour l'architecture matérielle.
*   **Persistance des Données :** Les paramètres de configuration, les noms des nœuds et les états critiques sont stockés dans une partition de la mémoire Flash non volatile.

#### **7.0 Alimentation et Exigences Environnementales**

*   **Tension d'Alimentation :** 5V DC (régulée) via connecteur USB Type-C.
*   **Consommation Électrique Nominale (Veille) :** ~150 mA.
*   **Consommation Électrique Maximale (Transmission LoRa +20dBm) :** ~400 mA.
*   **Température de Fonctionnement :** -20°C à +60°C.
*   **Humidité de Fonctionnement :** 5% à 95% sans condensation.
*   **Indice de Protection Requis pour le Boîtier Externe :** IP67.

---
**Fin de la Fiche Technique**
