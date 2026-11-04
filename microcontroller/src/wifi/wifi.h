#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

// Join the saved network, or open the setup hotspot (call before setupServer)
void setupWiFi();

// Keep the Wi-Fi connected, and log the address once connected (call on every loop iteration)
void manageWiFiConnection();

// Check whether the switch is connected to the Wi-Fi
bool isWiFiConnected();

// Check whether the setup hotspot is open
bool isSetupHotspotActive();

#endif