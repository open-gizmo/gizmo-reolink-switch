#ifndef SERVER_H
#define SERVER_H

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <gizmo/http.h>

// Standard API responses and CORS checks come from gizmo-common
using gizmo::ensureCorsAllowed;
using gizmo::sendCorsPreflightResponse;
using gizmo::sendErrorResponse;
using gizmo::sendSuccessResponse;

// Web server shared by the web app, its API and the Wi-Fi setup
extern AsyncWebServer server;

// Web server routes (the Wi-Fi setup and device routes are registered by gizmo-common)
void setupStatusGetRoute();
void setupNvrGetRoute();
void setupNvrPostRoute();
void setupNvrTestPostRoute();
void setupWebAppRoute();

// Build the state the web app shows: the timer, the alerts and the NVR
JsonDocument buildStatusData();

// Build the NVR login the web app shows (never the password)
JsonDocument buildNvrData();

// Register the routes and start the server (after the Wi-Fi setup registered its own)
void setupServer();

#endif