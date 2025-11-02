#ifndef ROLE_MANAGER_H
#define ROLE_MANAGER_H

#include <Preferences.h>

// Defines the possible roles a device can take.
enum DeviceRole {
  UNPROVISIONED,
  CENTRALE,
  AQUA_RESERV_PRO,
  WELLGUARD_PRO
};

class RoleManager {
public:
  RoleManager();
  DeviceRole loadRole();
  void saveRole(DeviceRole role);
  void clearRole();

private:
  Preferences preferences;
  const char* PREFS_NAMESPACE = "hydro_config";
  const char* ROLE_KEY = "device_role";
};

#endif // ROLE_MANAGER_H
