#pragma once
// Visible ISS passes over home with SGP4 (src/sgp4, MIT, Hopperpop).
// Platform independent so it can be checked on the desktop (tools/preview).
#include <time.h>
#include "radarview.h"
#include "src/sgp4/Sgp4.h"

// Loads the two TLE lines out of a celestrak response. Returns false if not found.
bool issLoadTle(Sgp4 &sat, const char *text);
// Next pass that can be seen with the naked eye (dark sky, sunlit ISS, at least 15 degrees up)
// within the next day. Fills the track and the pass window (unix seconds).
bool issFindPass(Sgp4 &sat, double lat, double lon, time_t now, IssInfo &info, time_t &start, time_t &end);
// Live position, "when" text and in-pass flag.
void issUpdateLive(Sgp4 &sat, time_t now, time_t start, time_t end, IssInfo &info);
