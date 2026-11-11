#pragma once
#include <Arduino.h>

void netInit();
// rescue = hotspot mode regardless of saved networks (3 quick power cycles).
void netStart(bool rescue);
void netStop();
void netLoop();
bool netActive();
bool netOnline();
int netClients();            // phones connected to the hotspot
uint32_t netUiServedAt();    // millis() when the app page was last loaded (0 = never)
String netHotspotName();  // connected to a saved network (internet)
