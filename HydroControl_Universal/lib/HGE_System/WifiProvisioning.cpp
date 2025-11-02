#include "WifiProvisioning.h"
#include <WiFi.h>
#include <LittleFS.h>
#include "RoleManager.h"
#include <ESPmDNS.h>


// Global instance to be accessible by the web server handler
static RoleManager roleManager_instance;

void startProvisioning() {
    static WifiProvisioning provisioning;
    provisioning.start();
}

void handleProvisioningRequest(AsyncWebServerRequest *request) {
    if (request->hasParam("role", true)) {
        String roleStr = request->getParam("role", true)->value();
        DeviceRole role = UNPROVISIONED;

        if (roleStr.equalsIgnoreCase("Centrale")) {
            role = CENTRALE;
        } else if (roleStr.equalsIgnoreCase("AquaReservPro")) {
            role = AQUA_RESERV_PRO;
        } else if (roleStr.equalsIgnoreCase("WellguardPro")) {
            role = WELLGUARD_PRO;
        }

        if (role != UNPROVISIONED) {
            roleManager_instance.saveRole(role);

            // Save network credentials if Centrale is selected
            if (role == CENTRALE && request->hasParam("ssid", true) && request->hasParam("password", true)) {
                String ssid = request->getParam("ssid", true)->value();
                String password = request->getParam("password", true)->value();

                Preferences prefs;
                prefs.begin("network_config", false);
                prefs.putString("ssid", ssid);
                prefs.putString("password", password);
                prefs.end();
            }

            // Save LoRa PSK
            if (request->hasParam("lora_psk", true)) {
                String lora_psk = request->getParam("lora_psk", true)->value();
                Preferences prefs;
                prefs.begin("security_config", false);
                prefs.putString("lora_psk", lora_psk);
                prefs.end();
            }

            request->send(200, "text/plain", "Configuration saved. The device will now restart.");
            delay(1000);
            ESP.restart();
        } else {
            request->send(400, "text/plain", "Invalid role selected.");
        }
    } else {
        request->send(400, "text/plain", "Role parameter is missing.");
    }
}


WifiProvisioning::WifiProvisioning() : server(80) {
    // Constructor
}

void WifiProvisioning::start() {
    setupAP();
    setupWebServer();
    // The web server runs asynchronously, so we just need to keep the task alive.
    // An empty loop with a delay is sufficient.
    while (true) {
        delay(1000);
    }
}

void WifiProvisioning::setupAP() {
    WiFi.softAP("HydroControl-Setup");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    if (MDNS.begin("hydrocontrol")) {
        Serial.println("MDNS responder started");
        MDNS.addService("http", "tcp", 80);
    }
}

void WifiProvisioning::setupWebServer() {
    if(!LittleFS.begin()){
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/configure", HTTP_POST, handleProvisioningRequest);

    server.begin();
}
