#include "RoleManager.h"

RoleManager::RoleManager() {
  // Constructor
}

DeviceRole RoleManager::loadRole() {
  if (!preferences.begin(PREFS_NAMESPACE, false)) {
    // Failed to open preferences, assume unprovisioned
    return UNPROVISIONED;
  }

  int roleInt = preferences.getInt(ROLE_KEY, -1);
  preferences.end();

  if (roleInt == -1) {
    return UNPROVISIONED;
  }

  return static_cast<DeviceRole>(roleInt);
}

void RoleManager::saveRole(DeviceRole role) {
  preferences.begin(PREFS_NAMESPACE, false);
  preferences.putInt(ROLE_KEY, static_cast<int>(role));
  preferences.end();
}

void RoleManager::clearRole() {
  preferences.begin(PREFS_NAMESPACE, false);
  preferences.remove(ROLE_KEY);
  preferences.end();
}
