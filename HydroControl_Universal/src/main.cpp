#include <Arduino.h>
#include "config.h"
#include "RoleManager.h"
#include "WatchdogManager.h"
#include "LedManager.h"
#include "WifiProvisioning.h"
#include "CentraleLogic.h"
#include "AquaReservLogic.h"
#include "WellguardLogic.h"

RoleManager roleManager;
DeviceRole currentRole;

// Instance pointers for logic modules
CentraleLogic* centrale = nullptr;
AquaReservLogic* aquaReserv = nullptr;
WellguardLogic* wellguard = nullptr;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  ledManager.begin();
  ledManager.setState(BOOTING);

  Serial.println("Booting HydroControl-GE Universal Firmware v3.0.0...");

  // Initialisation du watchdog avec un timeout de 15 secondes.
  WatchdogManager::initialize(15);

  currentRole = roleManager.loadRole();

  switch (currentRole) {
    case UNPROVISIONED:
      Serial.println("Device is not provisioned. Starting provisioning portal.");
      ledManager.setState(SETUP_MODE);
      startProvisioning();
      break;
    case CENTRALE:
      Serial.println("Role configured as: CENTRALE");
      centrale = new CentraleLogic();
      centrale->initialize();
      ledManager.setState(SYSTEM_OK);
      break;
    case AQUA_RESERV_PRO:
      Serial.println("Role configured as: AQUA_RESERV_PRO");
      aquaReserv = new AquaReservLogic();
      aquaReserv->initialize();
      ledManager.setState(SYSTEM_OK);
      break;
    case WELLGUARD_PRO:
      Serial.println("Role configured as: WELLGUARD_PRO");
      wellguard = new WellguardLogic();
      wellguard->initialize();
      ledManager.setState(SYSTEM_OK);
      break;
    default:
      Serial.println("Error: Unknown role found. Halting.");
      ledManager.setState(CRITICAL_ERROR);
      break;
  }
}

void loop() {
  // The main loop is managed by FreeRTOS tasks started by the logic modules.
  // This loop can be used for very low-priority background tasks if needed.
  vTaskDelay(portMAX_DELAY);
}
