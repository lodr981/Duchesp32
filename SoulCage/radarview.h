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
enum RadarMode : uint8_t { RADAR_OFF, RADAR_WEATHER, RADAR_PLANES, RADAR_FORECAST, RADAR_ISS };

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

// Daily forecast: [0] today, [1] tomorrow.
struct DayForecast {
  int code = -1;
  float tMax = 0, tMin = 0;
  int rainProb = 0;  // %
  float rainMm = 0;
};
struct Forecast {
  bool valid = false;
  DayForecast day[2];
};

// An aircraft flying right over home, with its route if known.
struct OverheadAlert {
  bool active = false;
  Plane plane;
  float distanceKm = 0;
  char typeName[24] = "";  // "Airbus A320"
  char from[28] = "";      // "Praha"
  char to[28] = "";        // "Londýn"
  char airline[32] = "";
};
// ICAO type designator -> friendly name ("B738" -> "Boeing 737"), or the code itself.
void aircraftTypeName(const char *icaoType, char *out, int cap);

// The International Space Station seen from home.
#define ISS_TRACK_POINTS 24
struct IssInfo {
  bool valid = false;    // orbit known and clock set
  bool hasPass = false;  // a visible pass within the next day
  bool inPass = false;   // that pass is happening now
  float azStart = 0, azEnd = 0, maxEl = 0;
  float track[ISS_TRACK_POINTS][2];  // azimuth, elevation (degrees) along the pass
  int trackCount = 0;
  char when[32] = "";    // "Za 12 min (19:42)", "Dnes v 19:42", "Zítra v 5:12"
  int minutesTo = 0;
  // live position
  float az = 0, el = 0, distKm = 0;
  bool sunlit = false;
  const char *status = "";
};

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
  // storm cells (strongest echoes) within the range
  bool storm = false;
  float stormKm = 0, stormBearing = 0;
  int stormTrend = 0;     // +1 coming closer, -1 moving away, 0 unknown
};

RainInfo rainInfo(const uint16_t *pixels, const GeoView &view);
// Fills info.stormTrend by comparing with an older frame (about 15 minutes before).
void stormTrend(RainInfo &newest, const RainInfo &older);
// Simple drawn weather icon (sun, clouds, rain, snow, storm, fog) for a WMO code.
void drawWeatherIcon(Canvas &c, float x, float y, float size, int code, float t);
const char *bearingName(float deg);  // "S", "SV", ... in Czech

class RadarScreens {
 public:
  // status: shown when there is nothing to draw ("Připojuji…", "Bez internetu"...)
  void drawWeather(Canvas &c, const GeoView &view, const WeatherFrame *frames, int count, const char *homeName,
                   const char *status, const CurrentWeather &now, float t);
  void drawPlanes(Canvas &c, const GeoView &view, const Plane *planes, int count, const char *homeName,
                  const char *status, const CurrentWeather &now, float t, const OverheadAlert *alert = nullptr);
  // evening = advice for tomorrow, otherwise for today
  void drawForecast(Canvas &c, const Forecast &f, const CurrentWeather &now, const char *homeName, bool evening,
                    float t);
  void drawIss(Canvas &c, const IssInfo &iss, float t);

 private:
  int shown = 0;      // animation frame
  float frameClock = 0, lastT = 0;
};
