#pragma once
// Boot greeting: the panda waves and a personal greeting bounces in.
#include "ghost.h"

#define GREETING_SECONDS 4.2f

// t = seconds since the greeting started. The ghost must be updated by the caller.
void drawGreeting(Canvas &c, Ghost &ghost, const GhostLook &look, const char *text, const char *subtitle, float t);
// Full-screen message with optional progress (0..100, <0 = none).
void drawMessage(Canvas &c, const char *title, const char *line, int progress);
