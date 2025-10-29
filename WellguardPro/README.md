# Module WellguardPro

## 1. Rôle et Fonctionnalités

Le module **WellguardPro** est l'actionneur du système. Son rôle est simple mais essentiel : recevoir les ordres de commande et piloter physiquement la pompe d'un puits.

- **Actionneur de Pompe** : Il est équipé d'un relais de puissance capable de commuter la charge électrique d'une pompe.
- **Récepteur LoRa** : Il écoute les commandes chiffrées envoyées par les modules `AquaReservPro` ou relayées par la `Centrale`.
- **Communication d'État** : Après chaque commande reçue et exécutée, il envoie un acquittement (ACK) à l'émetteur et un message de mise à jour de statut à la `Centrale` pour informer le tableau de bord.
- **Diagnostic Local** : Il héberge une page web simple pour un diagnostic rapide sur site.

## 2. Logique Opérationnelle

La logique du `WellguardPro` est directe :
1.  Il reçoit un paquet LoRa.
2.  Il le déchiffre avec la clé secrète partagée.
3.  Il vérifie que le message lui est bien destiné (en comparant l'ID cible avec son propre ID).
4.  Si c'est une commande de pompe (`CMD_PUMP_ON` ou `CMD_PUMP_OFF`), il active ou désactive le relais connecté au `RELAY_PIN`.
5.  Il envoie immédiatement un paquet `COMMAND_ACK` à l'expéditeur de la commande.
6.  Il envoie également un paquet `STATUS_UPDATE` à la `Centrale` pour que l'interface de supervision reflète le nouvel état (`ON` ou `OFF`).

## 3. Connexions Matérielles (Wiring)

- **`RELAY_PIN` (GPIO 23)** : Connecter la broche de commande du module relais. Le relais, à son tour, sera câblé pour commuter l'alimentation de la pompe du puits.
- **`BUTTON_PIN` (GPIO 22)** : Bien que le matériel dispose d'un bouton, celui-ci n'a pas de fonction assignée dans la version actuelle du firmware, car la logique manuelle est déportée sur les modules `AquaReservPro`.

## 4. Interface Web de Diagnostic

Chaque module `WellguardPro` héberge une page web accessible via son adresse IP. Cette interface est conçue pour le diagnostic et affiche :
- L'ID unique du module.
- L'état actuel du relais (`ON`/`OFF`).
- Le **RSSI** (indicateur de force du signal) de la dernière commande LoRa reçue, une information précieuse pour diagnostiquer des problèmes de communication.

## 5. Configuration Initiale

Le processus est identique à celui des autres modules. Le `WellguardPro` crée un point d'accès Wi-Fi **`HydroControl-Setup`** au premier démarrage, permettant de configurer le Wi-Fi local et la clé secrète LoRa.
