#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#include <Arduino.h>

// Ce gestionnaire encapsule le Task Watchdog Timer (TWDT) de l'ESP32.
// Chaque tâche FreeRTOS critique doit s'enregistrer et ensuite périodiquement
// "nourrir" (pet) le watchdog pour signaler qu'elle fonctionne correctement.
// Si une tâche enregistrée ne nourrit pas le watchdog dans le temps imparti,
// le système redémarrera automatiquement.

class WatchdogManager {
public:
    /**
     * @brief Initialise le Task Watchdog Timer.
     * Doit être appelé une seule fois au démarrage dans la fonction setup().
     * @param timeout_s Le temps en secondes avant que le watchdog ne se déclenche.
     */
    static void initialize(uint32_t timeout_s);

    /**
     * @brief Enregistre la tâche FreeRTOS en cours auprès du watchdog.
     * Chaque tâche critique doit appeler cette fonction une fois à son démarrage.
     */
    static void registerTask();

    /**
     * @brief "Nourrit" (réinitialise) le watchdog pour la tâche en cours.
     * Chaque tâche enregistrée doit appeler cette fonction périodiquement dans sa boucle principale.
     */
    static void pet();
};

#endif // WATCHDOG_MANAGER_H
