#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include <ESPAsyncWebServer.h>
#include "RoleManager.h"

void startProvisioning();
void handleProvisioningRequest(AsyncWebServerRequest *request);

class WifiProvisioning {
public:
  WifiProvisioning();
  void start();

private:
  AsyncWebServer server;
  RoleManager roleManager;

  void setupAP();
  void setupWebServer();
};

#endif // WIFI_PROVISIONING_H
