#pragma once
#include <Arduino.h>

void netInit();
// rescue = hotspot mode regardless of saved networks (3 quick power cycles).
void netStart(bool rescue);
void netStop();
void netLoop();
bool netActive();
