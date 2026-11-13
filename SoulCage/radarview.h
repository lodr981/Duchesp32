#pragma once
// Weather radar and aircraft radar screens for the round 240x240 display.
// Ideas and data sources follow MeteoPlaneRadar by chiptron.cz (MIT),
// redrawn for the pendant. Platform independent (see tools/preview).
#include <stdint.h>
#include "gfx.h"

#define RADAR_FRAMES 6
#define RADAR_MAX_PLANES 40
#define RADAR_PX 120  // weather frames are stored at half the display resolution

// Which radar screen the pendant shows (SoulCage.ino).
enum RadarMode : uint8_t { RADAR_OFF, RADAR_WEATHER, RADAR_PLANES };

// Map window: centre and the distance from the centre to the display rim.
struct GeoView {
  float lat = 50.6170f, lon = 15.1410f;  // Jenišovice u Jablonce nad Nisou
  float rangeKm = 50;
  void project(float plat, float plon, float &x, float &y) const;
  void unproject(float x, float y, float &plat, float &plon) const;
};

struct Plane {
  float lat, lon, track, altM, speedKmh;
  char callsign[10];
  char hex[8];   // ICAO address ("4b1805"), for links in the app
  char type[6];  // aircraft type ("A320"), may be empty
  bool hasTrack;
};

// Current weather at home (Open-Meteo).
struct CurrentWeather {
  bool valid = false;
  float tempC = 0;
  int code = -1;       // WMO weather code
  float windKmh = 0;
};
const char *weatherCodeName(int code);  // "jasno", "polojasno", "déšť"...

// One decoded radar picture, RADAR_PX x RADAR_PX covering the display (RGB565, 0 = no rain).
struct WeatherFrame {
  const uint16_t *pixels = nullptr;
  int minutesAgo = 0;
  char time[6] = "";  // local HH:MM
};

// Where the rain is relative to home, from the newest frame.
struct RainInfo {
  bool known = false;
  bool atHome = false;
  int level = 0;          // 0 none .. 4 storm, at home (or the nearest rain)
  float distanceKm = 0;   // to the nearest rain
  float bearingDeg = 0;
};

RainInfo rainInfo(const uint16_t *pixels, const GeoView &view);
const char *bearingName(float deg);  // "S", "SV", ... in Czech

class RadarScreens {
 public:
  // status: shown when there is nothing to draw ("Připojuji…", "Bez internetu"...)
  void drawWeather(Canvas &c, const GeoView &view, const WeatherFrame *frames, int count, const char *homeName,
                   const char *status, const CurrentWeather &now, float t);
  void drawPlanes(Canvas &c, const GeoView &view, const Plane *planes, int count, const char *homeName,
                  const char *status, const CurrentWeather &now, float t);

 private:
  int shown = 0;      // animation frame
  float frameClock = 0, lastT = 0;
};
