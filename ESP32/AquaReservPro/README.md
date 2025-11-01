# Module AquaReservPro

## 1. Rôle et Fonctionnalités

Le module **AquaReservPro** est le nœud de contrôle intelligent du système. Il est responsable de la surveillance d'un réservoir et de la mise en œuvre de la logique de commande pour le remplissage.

- **Mesure de Niveau** : Il lit en continu un capteur de niveau d'eau pour déterminer si le réservoir est `PLEIN` ou `VIDE`. Un filtre logiciel est appliqué pour ignorer les changements rapides dus aux remous, ne validant un nouvel état qu'après une stabilité de 5 secondes.
- **Logique de Contrôle Décentralisée** : La logique principale est exécutée localement, ce qui garantit le fonctionnement même en cas de perte de communication avec la centrale.
- **Gestion des Modes** : Il opère selon deux modes : `Automatique` (par défaut) et `Manuel`.
- **Communication Fiable** : Il est responsable d'envoyer les commandes de pompe de manière robuste au `WellguardPro` qui lui est assigné.
- **Persistance des États** : Le mode de fonctionnement et l'assignation du puits sont sauvegardés en mémoire non volatile pour survivre à une coupure de courant.

## 2. Logique Opérationnelle

### 2.1. Mode Automatique

C'est le mode par défaut. La logique est simple et efficace :
- Si le niveau détecté est `VIDE`, la commande de pompe est `ON`.
- Si le niveau détecté est `PLEIN`, la commande de pompe est `OFF`.

La commande n'est envoyée que lors d'un changement d'état.

**Note sur les Modes de Synchronisation Avancés :** Le cahier des charges mentionne des modes de synchronisation (`Maître/Esclave`, `Global`). Ces fonctionnalités ne sont pas implémentées dans la version actuelle mais l'architecture logicielle est conçue pour permettre leur ajout dans des versions futures.

### 2.2. Mode Manuel

L'utilisateur peut basculer entre les modes en appuyant sur le bouton physique du module.
- Une première pression passe du mode `AUTO` à `MANUAL`.
- En mode `MANUAL`, chaque pression inverse l'état de la pompe (`ON` -> `OFF` -> `ON`).
- **Sécurité** : Le passage à `ON` est bloqué si le capteur de niveau indique que le réservoir est déjà `PLEIN`.
- Une nouvelle pression sur le bouton repasse le système en mode `AUTO`.

## 3. Communication et Fiabilité

Lorsqu'une commande de pompe doit être envoyée, l'`AquaReservPro` utilise un protocole d'envoi fiable :
1.  Il envoie la commande chiffrée directement au `WellguardPro` assigné.
2.  Il attend un acquittement (ACK) pendant 2 secondes.
3.  En cas d'échec, il réessaie jusqu'à 3 fois.
4.  Si toutes les tentatives directes échouent, il envoie une demande de relais à la `Centrale`, qui se chargera de transmettre la commande.

## 4. Connexions Matérielles (Wiring)

- **`LEVEL_SENSOR_PIN` (GPIO 23)** : Connecter le capteur de niveau. Le système utilise une logique `INPUT_PULLUP`, donc le capteur doit connecter ce pin à la masse (GND) lorsqu'il détecte que le niveau est plein.
- **`BUTTON_PIN` (GPIO 22)** : Connecter un bouton-poussoir entre ce pin et la masse (GND) pour le contrôle manuel.

## 5. Interface Web de Diagnostic

Chaque module `AquaReservPro` héberge une petite page web accessible via son adresse IP. Cette page affiche en temps réel :
- L'ID unique du module.
- L'état du capteur de niveau (`PLEIN`/`VIDE`).
- Le mode de fonctionnement (`AUTO`/`MANUEL`).
- La dernière commande de pompe envoyée (`ON`/`OFF`).
- L'ID du `WellguardPro` qui lui est assigné.

## 6. Configuration Initiale

Le processus est identique à celui de la `Centrale`. Le module crée un point d'accès Wi-Fi **`HydroControl-Setup`** au premier démarrage, permettant de configurer le Wi-Fi local et la clé secrète LoRa.
