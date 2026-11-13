#include "radarview.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "map_data.h"

static const float RIM = 118;  // display radius in pixels
static const float DEG = 0.017453293f;

static float clampf(float v, float lo, float hi) {
  return v < lo ? lo : v > hi ? hi : v;
}

void GeoView::project(float plat, float plon, float &x, float &y) const {
  float k = RIM / rangeKm;
  x = 120 + (plon - lon) * 111.32f * cosf(lat * DEG) * k;
  y = 120 - (plat - lat) * 111.32f * k;
}

void GeoView::unproject(float x, float y, float &plat, float &plon) const {
  float k = RIM / rangeKm;
  plat = lat - (y - 120) / (111.32f * k);
  plon = lon + (x - 120) / (111.32f * cosf(lat * DEG) * k);
}

const char *bearingName(float deg) {
  static const char *const names[8] = {"S", "SV", "V", "JV", "J", "JZ", "Z", "SZ"};
  int i = (int)floorf(fmodf(deg + 22.5f + 360, 360) / 45) % 8;
  return names[i];
}

// Rough intensity from the radar palette: blue light rain .. red/pink storm.
static int rainLevel(uint16_t v) {
  if (!v) return 0;
  int r = (v >> 11) << 3, g = ((v >> 5) & 63) << 2, b = (v & 31) << 3;
  if (r > 200 && b > 150 && g < 170) return 4;  // pink/purple: extreme
  if (r > 150 && g < 110) return 4;              // red
  if (r > 180 && g > 110) return 3;              // yellow/orange
  if (g > b) return 2;                           // green
  return 1;                                      // blue
}

RainInfo rainInfo(const uint16_t *px, const GeoView &view) {
  RainInfo info;
  if (!px) return info;
  info.known = true;
  float best = 1e9f, bestStorm = 1e9f;
  for (int sy = 0; sy < RADAR_PX; sy++) {
    for (int sx = 0; sx < RADAR_PX; sx++) {
      int level = rainLevel(px[sy * RADAR_PX + sx]);
      float x = (sx + 0.5f) * 240 / RADAR_PX, y = (sy + 0.5f) * 240 / RADAR_PX;
      if (!level) continue;
      float dx = x - 120, dy = y - 120, d = sqrtf(dx * dx + dy * dy);
      if (d > RIM) continue;
      if (level >= 4 && d < bestStorm) {
        bestStorm = d;
        info.stormBearing = fmodf(atan2f(dx, -dy) / DEG + 360, 360);
      }
      if (d < best || (d - best < 3 && level > info.level)) {
        best = d;
        info.level = level;
        info.bearingDeg = fmodf(atan2f(dx, -dy) / DEG + 360, 360);
      }
    }
  }
  if (best < 1e9f) {
    info.distanceKm = best / RIM * view.rangeKm;
    info.atHome = info.distanceKm < 3;
  }
  if (bestStorm < 1e9f) {
    info.storm = true;
    info.stormKm = bestStorm / RIM * view.rangeKm;
  }
  return info;
}

void stormTrend(RainInfo &newest, const RainInfo &older) {
  newest.stormTrend = 0;
  if (!newest.storm || !older.storm) return;
  float d = newest.stormKm - older.stormKm;
  if (d < -2) newest.stormTrend = 1;
  else if (d > 2) newest.stormTrend = -1;
}

// ------------------------------------------------------------------ icons

static void cloudShape(Canvas &c, float x, float y, float s, Color col, uint8_t a) {
  c.circle(x - 0.45f * s, y + 0.1f * s, 0.32f * s, col, a);
  c.circle(x + 0.05f * s, y - 0.12f * s, 0.42f * s, col, a);
  c.circle(x + 0.5f * s, y + 0.12f * s, 0.3f * s, col, a);
  c.roundRect(x - 0.75f * s, y + 0.05f * s, 1.5f * s, 0.38f * s, 0.19f * s, col, a);
}

static void sunShape(Canvas &c, float x, float y, float s, float t) {
  Color sun = {255, 210, 80};
  c.glow(x, y, s * 0.9f, s * 0.9f, {255, 180, 60}, 90);
  for (int i = 0; i < 8; i++) {
    float an = i * 45 * DEG + t * 0.4f;
    c.line(x + cosf(an) * s * 0.5f, y + sinf(an) * s * 0.5f, x + cosf(an) * s * 0.72f, y + sinf(an) * s * 0.72f,
           s * 0.09f, sun);
  }
  c.circle(x, y, s * 0.36f, sun);
}

static void boltShape(Canvas &c, float x, float y, float s, Color col, uint8_t a) {
  c.triangle(x + 0.1f * s, y - 0.5f * s, x - 0.25f * s, y + 0.08f * s, x + 0.05f * s, y + 0.08f * s, col, a);
  c.triangle(x - 0.05f * s, y - 0.05f * s, x + 0.25f * s, y - 0.05f * s, x - 0.12f * s, y + 0.55f * s, col, a);
}

void drawWeatherIcon(Canvas &c, float x, float y, float s, int code, float t) {
  Color cloud = {225, 230, 245}, dark = {150, 160, 185};
  if (code < 0) return;
  if (code <= 1) {
    sunShape(c, x, y, s, t);
    return;
  }
  if (code == 2) {
    sunShape(c, x - 0.25f * s, y - 0.25f * s, s * 0.8f, t);
    cloudShape(c, x + 0.1f * s, y + 0.15f * s, s * 0.75f, cloud, 255);
    return;
  }
  if (code == 45 || code == 48) {
    cloudShape(c, x, y - 0.15f * s, s * 0.8f, dark, 255);
    for (int i = 0; i < 3; i++) {
      float yy = y + (0.3f + i * 0.17f) * s;
      c.line(x - 0.6f * s + i * 0.1f * s, yy, x + 0.6f * s - i * 0.1f * s, yy, s * 0.07f, cloud, 200);
    }
    return;
  }
  bool rain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
  bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
  bool storm = code >= 95;
  cloudShape(c, x, y - 0.2f * s, s * 0.85f, rain || snow || storm ? dark : cloud, 255);
  if (rain || storm) {
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(t * 1.2f + i * 0.33f, 1.0f);
      float xx = x + (i - 1) * 0.32f * s - ph * 0.08f * s, yy = y + (0.25f + ph * 0.35f) * s;
      c.line(xx, yy, xx - 0.06f * s, yy + 0.16f * s, s * 0.07f, {110, 180, 255}, (uint8_t)(255 * (1 - ph * 0.6f)));
    }
  }
  if (snow) {
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(t * 0.5f + i * 0.33f, 1.0f);
      c.circle(x + (i - 1) * 0.32f * s + sinf(t * 2 + i) * 0.05f * s, y + (0.28f + ph * 0.35f) * s, s * 0.06f,
               {240, 245, 255});
    }
  }
  if (storm) {
    bool flash = fmodf(t, 2.2f) < 0.25f;
    boltShape(c, x + 0.05f * s, y + 0.35f * s, s * 0.6f, {255, 225, 90}, flash ? 255 : 200);
  }
}

// ------------------------------------------------------------------ aircraft types

void aircraftTypeName(const char *t, char *out, int cap) {
  static const struct {
    const char *code, *name;
  } types[] = {
      {"A318", "Airbus A318"},   {"A319", "Airbus A319"},     {"A320", "Airbus A320"},    {"A321", "Airbus A321"},
      {"A19N", "Airbus A319neo"}, {"A20N", "Airbus A320neo"}, {"A21N", "Airbus A321neo"}, {"A332", "Airbus A330"},
      {"A333", "Airbus A330"},   {"A338", "Airbus A330neo"},  {"A339", "Airbus A330neo"}, {"A343", "Airbus A340"},
      {"A346", "Airbus A340"},   {"A359", "Airbus A350"},     {"A35K", "Airbus A350"},    {"A388", "Airbus A380"},
      {"BCS1", "Airbus A220"},   {"BCS3", "Airbus A220"},     {"B733", "Boeing 737"},     {"B734", "Boeing 737"},
      {"B735", "Boeing 737"},    {"B736", "Boeing 737"},      {"B737", "Boeing 737"},     {"B738", "Boeing 737"},
      {"B739", "Boeing 737"},    {"B37M", "Boeing 737 MAX"},  {"B38M", "Boeing 737 MAX"}, {"B39M", "Boeing 737 MAX"},
      {"B3XM", "Boeing 737 MAX"}, {"B744", "Boeing 747"},     {"B748", "Boeing 747-8"},   {"B752", "Boeing 757"},
      {"B763", "Boeing 767"},    {"B764", "Boeing 767"},      {"B772", "Boeing 777"},     {"B77L", "Boeing 777"},
      {"B77W", "Boeing 777"},    {"B778", "Boeing 777X"},     {"B779", "Boeing 777X"},    {"B788", "Boeing 787"},
      {"B789", "Boeing 787"},    {"B78X", "Boeing 787"},      {"E170", "Embraer 170"},    {"E75L", "Embraer 175"},
      {"E190", "Embraer 190"},   {"E195", "Embraer 195"},     {"E290", "Embraer E2"},     {"E295", "Embraer E2"},
      {"AT72", "ATR 72"},        {"AT75", "ATR 72"},          {"AT76", "ATR 72"},         {"AT45", "ATR 42"},
      {"DH8D", "Dash 8"},        {"CRJ7", "Bombardier CRJ"},  {"CRJ9", "Bombardier CRJ"}, {"CRJX", "Bombardier CRJ"},
      {"C172", "Cessna 172"},    {"C152", "Cessna 152"},      {"C182", "Cessna 182"},     {"P28A", "Piper"},
      {"SR22", "Cirrus SR22"},   {"PC12", "Pilatus PC-12"},   {"C68A", "Cessna Latitude"}, {"GLF6", "Gulfstream"},
      {"A400", "Airbus A400M"},  {"C130", "Hercules C-130"},  {"C30J", "Hercules C-130"}, {"EC35", "vrtulník"},
      {"EC45", "vrtulník"},        {"H145", "vrtulník"},       {"H135", "vrtulník"},
      {"A139", "vrtulník"},      {"R44", "vrtulník"},         {"L410", "Let L-410"},      {"Z42", "Zlín"},
      {"BE20", "King Air"},      {"A124", "Antonov An-124"},  {"B77F", "Boeing 777F"},
  };
  out[0] = 0;
  if (!t || !*t) return;
  for (const auto &e : types) {
    if (strcmp(e.code, t) == 0) {
      snprintf(out, cap, "%s", e.name);
      return;
    }
  }
  snprintf(out, cap, "%s", t);
}

// ------------------------------------------------------------------ map

struct LabelBox {
  float x0, y0, x1, y1;
};

static void drawMap(Canvas &c, const GeoView &view, Color border, uint8_t borderA, Color town, uint8_t townA,
                    const char *homeName) {
  for (int r = 0; r < MAP_RING_COUNT; r++) {
    float px = 0, py = 0;
    bool prevIn = false;
    for (int i = MAP_RINGS[r]; i < MAP_RINGS[r + 1]; i++) {
      float lat = MAP_LAT0 + MAP_PTS[i][0] * MAP_STEP, lon = MAP_LON0 + MAP_PTS[i][1] * MAP_STEP;
      float x, y;
      view.project(lat, lon, x, y);
      bool in = (x - 120) * (x - 120) + (y - 120) * (y - 120) < 150 * 150;
      if (i > MAP_RINGS[r] && (in || prevIn)) c.line(px, py, x, y, 1.4f, border, borderA);
      px = x, py = y, prevIn = in;
    }
  }
  int maxTier = view.rangeKm <= 30 ? 3 : view.rangeKm <= 60 ? 2 : 1;
  LabelBox boxes[24];
  int boxCount = 0;
  // home label first so towns do not cover it
  if (homeName && *homeName) {
    int w = Canvas::textWidth(homeName, FONT_TINY);
    boxes[boxCount++] = {120 - w / 2.0f - 2, 125, 120 + w / 2.0f + 2, 140};
  }
  for (const MapTown &t : MAP_TOWNS) {
    if (t.tier > maxTier) continue;
    float x, y;
    view.project(t.lat, t.lon, x, y);
    if ((x - 120) * (x - 120) + (y - 120) * (y - 120) > 104 * 104) continue;
    if (fabsf(x - 120) < 4 && fabsf(y - 120) < 4) continue;  // that is home
    int w = Canvas::textWidth(t.name, FONT_TINY);
    bool right = x + 5 + w < 225;
    LabelBox b = {right ? x + 4 : x - 4 - w, y - 8, right ? x + 4 + w : x - 4, y + 6};
    bool overlap = false;
    for (int i = 0; i < boxCount; i++) {
      overlap |= b.x0 < boxes[i].x1 && b.x1 > boxes[i].x0 && b.y0 < boxes[i].y1 && b.y1 > boxes[i].y0;
    }
    c.circle(x, y, 1.8f, town, townA);
    if (overlap || boxCount == 24) continue;
    boxes[boxCount++] = b;
    c.text(t.name, right ? x + 4 : x - 4, y - 8, FONT_TINY, town, townA, right ? ALIGN_LEFT : ALIGN_RIGHT);
  }
}

static void drawHome(Canvas &c, const char *name, float t, Color col) {
  float pulse = fmodf(t, 1.6f) / 1.6f;
  c.ring(120, 120, 4 + 10 * pulse, 1.5f, col, (uint8_t)(200 * (1 - pulse)));
  c.circle(120, 120, 3.5f, col);
  if (name && *name) c.text(name, 120, 125, FONT_TINY, {255, 255, 255}, 230);
}

static void plate(Canvas &c, float y, float w, float h) {
  c.roundRect(120 - w / 2, y, w, h, h / 2, {6, 6, 12}, 190);
}

// ------------------------------------------------------------------ weather

const char *weatherCodeName(int code) {
  if (code < 0) return "";
  if (code == 0) return "jasno";
  if (code == 1) return "skoro jasno";
  if (code == 2) return "polojasno";
  if (code == 3) return "zataženo";
  if (code == 45 || code == 48) return "mlha";
  if (code >= 51 && code <= 57) return "mrholí";
  if (code == 66 || code == 67) return "mrznoucí déšť";
  if (code >= 61 && code <= 65) return code == 65 ? "silný déšť" : "déšť";
  if (code >= 71 && code <= 77) return "sněží";
  if (code >= 80 && code <= 82) return "přeháňky";
  if (code == 85 || code == 86) return "sněhové přeháňky";
  if (code >= 95) return "bouřka";
  return "";
}

static Color tempColor(float c) {
  if (c <= 0) return {140, 200, 255};
  if (c < 10) return {190, 225, 255};
  if (c < 20) return {200, 255, 210};
  if (c < 28) return {255, 225, 130};
  return {255, 160, 110};
}

// "7 °C" with the sky condition next to it, on a plate under the top line.
static void drawTemperature(Canvas &c, const CurrentWeather &now, float y) {
  if (!now.valid) return;
  char temp[16];
  snprintf(temp, sizeof(temp), "%d °C", (int)lroundf(now.tempC));
  const char *sky = weatherCodeName(now.code);
  int tw = Canvas::textWidth(temp, FONT_SMALL), sw = *sky ? Canvas::textWidth(sky, FONT_TINY) + 6 : 0;
  float w = tw + sw + 20.0f;
  c.roundRect(120 - w / 2, y, w, 22, 11, {6, 6, 12}, 210);
  float x = 120 - (tw + sw) / 2.0f;
  c.text(temp, x, y + 2, FONT_SMALL, tempColor(now.tempC), 255, ALIGN_LEFT);
  if (*sky) c.text(sky, x + tw + 6, y + 6, FONT_TINY, {200, 205, 230}, 230, ALIGN_LEFT);
}

void RadarScreens::drawWeather(Canvas &c, const GeoView &view, const WeatherFrame *frames, int count,
                               const char *homeName, const char *status, const CurrentWeather &now, float t) {
  float dt = lastT ? clampf(t - lastT, 0, 0.5f) : 0;
  lastT = t;
  c.gradient({12, 16, 28}, {4, 6, 12});
  char buf[48];

  if (count > 0) {
    // Animation: every frame 0.6 s, the newest held for 2 s.
    if (shown >= count) shown = count - 1;
    frameClock += dt;
    float hold = shown == count - 1 ? 2.0f : 0.6f;
    if (frameClock > hold) {
      frameClock = 0;
      shown = (shown + 1) % count;
    }
    const uint16_t *px = frames[shown].pixels;
    if (px) {
      const int k = 240 / RADAR_PX;  // each stored pixel covers k x k display pixels
      for (int i = 0; i < RADAR_PX * RADAR_PX; i++) {
        uint16_t v = px[i];
        if (!v) continue;
        Color col = {(uint8_t)((v >> 11) << 3), (uint8_t)(((v >> 5) & 63) << 2), (uint8_t)((v & 31) << 3)};
        int x0 = (i % RADAR_PX) * k, y0 = (i / RADAR_PX) * k;
        for (int yy = 0; yy < k; yy++)
          for (int xx = 0; xx < k; xx++) c.blend(x0 + xx, y0 + yy, col, 225);
      }
      // lightning over the storm cells: a few flickering bolts
      int bolts = 0;
      for (int sy = 2; sy < RADAR_PX && bolts < 8; sy += 7) {
        for (int sx = 2; sx < RADAR_PX && bolts < 8; sx += 7) {
          if (rainLevel(px[sy * RADAR_PX + sx]) < 4) continue;
          float x = sx * 240.0f / RADAR_PX, y = sy * 240.0f / RADAR_PX;
          if ((x - 120) * (x - 120) + (y - 120) * (y - 120) > RIM * RIM) continue;
          bolts++;
          float ph = fmodf(t * 0.9f + (sx * 13 + sy * 7) % 17 * 0.13f, 1.6f);
          uint8_t a = ph < 0.12f ? 255 : ph < 0.3f ? 120 : 0;
          if (!a) continue;
          c.glow(x, y, 12, 12, {255, 255, 200}, a / 3);
          boltShape(c, x, y, 16, {255, 240, 120}, a);
        }
      }
    }
  }
  drawMap(c, view, {120, 140, 185}, 210, {190, 195, 220}, 220, homeName);
  drawHome(c, homeName, t, {255, 110, 190});

  if (count == 0) {
    plate(c, 150, 170, 28);
    c.text(status && *status ? status : "Načítám radar…", 120, 155, FONT_SMALL, {230, 220, 255});
    c.text("Meteoradar", 120, 6, FONT_SMALL, {255, 255, 255});
    drawTemperature(c, now, 28);
    return;
  }
  const WeatherFrame &f = frames[shown];
  if (f.minutesAgo > 0) {
    snprintf(buf, sizeof(buf), "%s  -%d min", f.time, f.minutesAgo);
  } else {
    snprintf(buf, sizeof(buf), "%s  teď", f.time);
  }
  c.text(buf, 120, 6, FONT_SMALL, {255, 255, 255});
  drawTemperature(c, now, 28);

  // Rain summary from the newest frame (computed once per new frame).
  static const uint16_t *cachedFor = nullptr;
  static RainInfo info;
  if (cachedFor != frames[count - 1].pixels) {
    cachedFor = frames[count - 1].pixels;
    info = rainInfo(cachedFor, view);
    if (count >= 2) stormTrend(info, rainInfo(frames[count >= 4 ? count - 4 : 0].pixels, view));
  }
  const char *msg;
  if (info.storm && !(info.atHome && info.level >= 4)) {
    snprintf(buf, sizeof(buf), "Bouřka %.0f km %s%s", info.stormKm, bearingName(info.stormBearing),
             info.stormTrend > 0 ? " · blíží se" : info.stormTrend < 0 ? " · odchází" : "");
    msg = buf;
  } else if (!info.known || info.level == 0) {
    msg = "Bez srážek v okolí";
  } else if (info.atHome) {
    msg = info.level >= 4 ? "Bouřka u nás!" : info.level >= 3 ? "U nás silně prší" : "U nás prší";
  } else {
    snprintf(buf, sizeof(buf), "%s %.0f km %s", info.level >= 4 ? "Bouřka" : "Déšť", info.distanceKm,
             bearingName(info.bearingDeg));
    msg = buf;
  }
  Color mc = info.atHome || info.level >= 4 || info.storm ? Color{255, 200, 120} : Color{220, 230, 255};
  FontId mf = Canvas::textWidth(msg, FONT_SMALL) > 196 ? FONT_TINY : FONT_SMALL;
  plate(c, 182, Canvas::textWidth(msg, mf) + 22.0f, 24);
  c.text(msg, 120, mf == FONT_SMALL ? 185 : 188, mf, mc);

  // frame dots and range
  for (int i = 0; i < count; i++) {
    float x = 120 + (i - (count - 1) / 2.0f) * 9;
    c.circle(x, 212, i == shown ? 2.8f : 1.8f, i == shown ? Color{255, 220, 120} : Color{140, 140, 170});
  }
  snprintf(buf, sizeof(buf), "%.0f km", view.rangeKm);
  c.text(buf, 120, 219, FONT_TINY, {170, 170, 200});
}

// ------------------------------------------------------------------ aircraft

static Color altitudeColor(float m) {
  if (m < 1000) return {255, 170, 80};
  if (m < 3000) return {255, 230, 110};
  if (m < 8000) return {120, 255, 150};
  return {130, 220, 255};
}

static void drawPlaneIcon(Canvas &c, float x, float y, float trackDeg, bool hasTrack, Color col, uint8_t a, float s) {
  if (!hasTrack) {
    c.circle(x, y, 3 * s, col, a);
    return;
  }
  float an = trackDeg * DEG, fx = sinf(an), fy = -cosf(an), rx = -fy, ry = fx;  // forward / right
  auto P = [&](float f, float r, float &ox, float &oy) {
    ox = x + (fx * f + rx * r) * s;
    oy = y + (fy * f + ry * r) * s;
  };
  float ax, ay, bx, by, cx2, cy2, dx, dy;
  P(6, 0, ax, ay);
  P(-5, 0, bx, by);
  c.line(ax, ay, bx, by, 2.2f * s, col, a);  // fuselage
  P(1, -6, cx2, cy2);
  P(1, 6, dx, dy);
  float wx, wy;
  P(2.5f, 0, wx, wy);
  c.triangle(cx2, cy2, dx, dy, wx, wy, col, a);  // wings
  P(-5, -3, cx2, cy2);
  P(-5, 3, dx, dy);
  P(-3, 0, wx, wy);
  c.triangle(cx2, cy2, dx, dy, wx, wy, col, a);  // tail
}

// "Right above you!": the plane highlighted, its type and route.
static void drawOverhead(Canvas &c, const GeoView &view, const Plane *planes, int count, const OverheadAlert &a,
                         float t) {
  Plane p = a.plane;
  for (int i = 0; i < count; i++) {
    if (a.plane.hex[0] && strcmp(planes[i].hex, a.plane.hex) == 0) p = planes[i];  // fresh position
  }
  float x, y;
  view.project(p.lat, p.lon, x, y);
  for (int k = 0; k < 2; k++) {
    float ph = fmodf(t * 0.8f + k * 0.5f, 1.0f);
    c.ring(x, y, 8 + ph * 34, 2.2f, {255, 220, 120}, (uint8_t)(220 * (1 - ph)));
  }
  drawPlaneIcon(c, x, y, p.track, p.hasTrack, {255, 230, 140}, 255, 1.6f);
  float pulse = 0.5f + 0.5f * sinf(t * 6);
  plate(c, 14, 150, 28);
  c.text("Nad tebou!", 120, 18, FONT_SMALL, {255, (uint8_t)(200 + 40 * pulse), 120});
  char line1[48], line2[64];
  snprintf(line1, sizeof(line1), "%s", a.typeName[0] ? a.typeName : p.callsign[0] ? p.callsign : "Letadlo");
  if (a.from[0] && a.to[0]) {
    snprintf(line2, sizeof(line2), "%s → %s", a.from, a.to);
  } else if (a.airline[0]) {
    snprintf(line2, sizeof(line2), "%s", a.airline);
  } else {
    snprintf(line2, sizeof(line2), "%s  %.1f km výšky", p.callsign[0] ? p.callsign : "", p.altM / 1000);
  }
  FontId f2 = Canvas::textWidth(line2, FONT_SMALL) > 190 ? FONT_TINY : FONT_SMALL;
  float w = Canvas::textWidth(line1, FONT_SMALL);
  float w2 = Canvas::textWidth(line2, f2);
  plate(c, 170, (w > w2 ? w : w2) + 24, 46);
  c.text(line1, 120, 173, FONT_SMALL, {255, 255, 255});
  c.text(line2, 120, f2 == FONT_SMALL ? 192 : 195, f2, {255, 225, 150});
  snprintf(line1, sizeof(line1), "%.1f km výšky · %.0f km/h", p.altM / 1000, p.speedKmh);
  c.text(line1, 120, 219, FONT_TINY, {200, 255, 210}, 220);
}

static void drawPlanesTemp(Canvas &c, const CurrentWeather &now) {
  if (!now.valid) return;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d °C", (int)lroundf(now.tempC));
  c.text(buf, 120, 216, FONT_TINY, tempColor(now.tempC), 230);
}

void RadarScreens::drawPlanes(Canvas &c, const GeoView &view, const Plane *planes, int count, const char *homeName,
                              const char *status, const CurrentWeather &now, float t, const OverheadAlert *alert) {
  c.fill({2, 10, 6});
  c.glow(120, 120, 135, 135, {12, 70, 34}, 130);
  drawMap(c, view, {40, 120, 70}, 150, {90, 170, 110}, 190, nullptr);
  Color grid = {50, 170, 90};
  char buf[48];
  for (int k = 1; k <= 3; k++) {
    float r = RIM * k / 3 - (k == 3 ? 2 : 0);
    c.ring(120, 120, r, 1.2f, grid, 130);
    snprintf(buf, sizeof(buf), "%.0f km", view.rangeKm * k / 3);
    if (k < 3) c.text(buf, 120 + r * 0.7071f + 2, 120 - r * 0.7071f - 12, FONT_TINY, grid, 170, ALIGN_LEFT);
  }
  c.line(120, 4, 120, 236, 1, grid, 50);
  c.line(4, 120, 236, 120, 1, grid, 50);
  c.text("S", 120, 3, FONT_TINY, grid, 200);

  // sweep (one turn in 4 s)
  float sweep = fmodf(t * 90, 360);
  float a0 = (sweep - 28) * DEG, a1 = sweep * DEG;
  c.triangle(120, 120, 120 + RIM * sinf(a0), 120 - RIM * cosf(a0), 120 + RIM * sinf(a1), 120 - RIM * cosf(a1),
             {80, 255, 140}, 38);
  c.line(120, 120, 120 + RIM * sinf(a1), 120 - RIM * cosf(a1), 2, {140, 255, 170}, 190);

  drawHome(c, homeName, t, {255, 110, 190});

  if (count == 0 && alert && alert->active) {
    drawOverhead(c, view, planes, 0, *alert, t);
    return;
  }
  if (count == 0) {
    plate(c, 150, 190, 28);
    c.text(status && *status ? status : "Žádná letadla v okolí", 120, 155, FONT_SMALL, {200, 255, 210});
    c.text("Letadla", 120, 22, FONT_SMALL, {200, 255, 210});
    drawPlanesTemp(c, now);
    return;
  }

  // nearest first for the labels
  int order[RADAR_MAX_PLANES];
  float dist[RADAR_MAX_PLANES];
  int n = count < RADAR_MAX_PLANES ? count : RADAR_MAX_PLANES;
  for (int i = 0; i < n; i++) {
    order[i] = i;
    float dx = (planes[i].lon - view.lon) * 111.32f * cosf(view.lat * DEG), dy = (planes[i].lat - view.lat) * 111.32f;
    dist[i] = sqrtf(dx * dx + dy * dy);
  }
  for (int i = 1; i < n; i++) {
    for (int j = i; j > 0 && dist[order[j]] < dist[order[j - 1]]; j--) {
      int tmp = order[j];
      order[j] = order[j - 1];
      order[j - 1] = tmp;
    }
  }
  int visible = 0;
  LabelBox boxes[8];
  int boxCount = 0;
  if (homeName && *homeName) {
    int w = Canvas::textWidth(homeName, FONT_TINY);
    boxes[boxCount++] = {120 - w / 2.0f - 2, 124, 120 + w / 2.0f + 2, 140};
  }
  for (int k = n - 1; k >= 0; k--) {  // far ones first, nearest drawn on top
    const Plane &p = planes[order[k]];
    float x, y;
    view.project(p.lat, p.lon, x, y);
    float dx = x - 120, dy = y - 120;
    if (dx * dx + dy * dy > (RIM - 4) * (RIM - 4)) continue;
    visible++;
    // afterglow: brighter just after the sweep passed
    float ang = fmodf(atan2f(dx, -dy) / DEG + 360, 360);
    float since = fmodf(sweep - ang + 360, 360);
    float glow = 0.55f + 0.45f * expf(-since / 90);
    Color col = altitudeColor(p.altM);
    c.glow(x, y, 9, 9, col, (uint8_t)(90 * glow));
    drawPlaneIcon(c, x, y, p.track, p.hasTrack, col, (uint8_t)(255 * glow), 1.0f);
    if (k < 5 && !(alert && alert->active)) {
      const char *name = p.callsign[0] ? p.callsign : "?";
      char alt[16];
      snprintf(alt, sizeof(alt), "%.1f km", p.altM / 1000);
      int w = Canvas::textWidth(name, FONT_TINY);
      bool right = x + 8 + w < 228;
      LabelBox b = {right ? x + 7 : x - 7 - w, y - 12, right ? x + 7 + w : x - 7, y + 12};
      bool overlap = false;
      for (int i = 0; i < boxCount; i++) {
        overlap |= b.x0 < boxes[i].x1 && b.x1 > boxes[i].x0 && b.y0 < boxes[i].y1 && b.y1 > boxes[i].y0;
      }
      if (!overlap && boxCount < 8) {
        boxes[boxCount++] = b;
        c.text(name, right ? x + 7 : x - 7, y - 12, FONT_TINY, {230, 255, 235}, 235, right ? ALIGN_LEFT : ALIGN_RIGHT);
        c.text(alt, right ? x + 7 : x - 7, y, FONT_TINY, col, 220, right ? ALIGN_LEFT : ALIGN_RIGHT);
      }
    }
  }
  if (alert && alert->active) {
    drawOverhead(c, view, planes, count, *alert, t);
    return;
  }
  snprintf(buf, sizeof(buf), visible == 1 ? "%d letadlo" : visible >= 2 && visible <= 4 ? "%d letadla" : "%d letadel",
           visible);
  c.text(buf, 120, 20, FONT_SMALL, {200, 255, 210});
  // nearest aircraft at the bottom
  const Plane &near = planes[order[0]];
  float bdx = (near.lon - view.lon) * 111.32f * cosf(view.lat * DEG), bdy = (near.lat - view.lat) * 111.32f;
  float bearing = fmodf(atan2f(bdx, bdy) / DEG + 360, 360);
  snprintf(buf, sizeof(buf), "%s  %.0f km %s", near.callsign[0] ? near.callsign : "?", dist[order[0]],
           bearingName(bearing));
  plate(c, 196, Canvas::textWidth(buf, FONT_TINY) + 18.0f, 18);
  c.text(buf, 120, 198, FONT_TINY, {230, 255, 235});
  drawPlanesTemp(c, now);
}

// ------------------------------------------------------------------ forecast

static Color tempColorFor(float c) {
  return tempColor(c);
}

void RadarScreens::drawForecast(Canvas &c, const Forecast &f, const CurrentWeather &now, const char *homeName,
                                bool evening, float t) {
  c.gradient({22, 34, 70}, {8, 10, 24});
  char buf[48];
  snprintf(buf, sizeof(buf), "%s", homeName && *homeName ? homeName : "Předpověď");
  c.text(buf, 120, 8, FONT_SMALL, {255, 255, 255});
  if (!f.valid) {
    plate(c, 150, 190, 28);
    c.text("Načítám předpověď…", 120, 155, FONT_SMALL, {230, 220, 255});
    drawWeatherIcon(c, 120, 95, 60, 2, t);
    return;
  }
  if (now.valid) {
    snprintf(buf, sizeof(buf), "teď %d °C", (int)lroundf(now.tempC));
    c.text(buf, 120, 30, FONT_TINY, tempColorFor(now.tempC), 230);
  }
  int focus = evening ? 1 : 0;
  const char *names[2] = {"Dnes", "Zítra"};
  for (int i = 0; i < 2; i++) {
    const DayForecast &d = f.day[i];
    float x = i == 0 ? 72 : 168;
    if (i == focus) c.roundRect(x - 44, 48, 88, 132, 18, {255, 255, 255}, 22);
    c.text(names[i], x, 52, FONT_SMALL, i == focus ? Color{255, 220, 120} : Color{200, 205, 230});
    drawWeatherIcon(c, x, 100, 40, d.code, t + i * 0.7f);
    snprintf(buf, sizeof(buf), "%d°", (int)lroundf(d.tMax));
    c.text(buf, x, 122, FONT_BIG, tempColorFor(d.tMax));
    snprintf(buf, sizeof(buf), "noc %d°", (int)lroundf(d.tMin));
    c.text(buf, x, 154, FONT_TINY, {180, 190, 220});
    if (d.rainProb >= 20 || d.rainMm >= 0.5f) {
      snprintf(buf, sizeof(buf), "déšť %d %%", d.rainProb);
      c.text(buf, x, 166, FONT_TINY, {120, 190, 255});
    }
  }
  // advice for the focused day
  const DayForecast &d = f.day[focus];
  bool snow = (d.code >= 71 && d.code <= 77) || d.code == 85 || d.code == 86;
  const char *msg;
  if (d.code >= 95) msg = evening ? "Zítra bouřky – pozor!" : "Dnes bouřky – pozor!";
  else if (snow) msg = evening ? "Zítra sněží – boby!" : "Dnes sněží – boby!";
  else if (d.rainProb >= 50 || d.rainMm >= 2) msg = "Vezmi si deštník!";
  else if (d.tMax <= 0) msg = "Mrzne – vezmi čepici!";
  else if (d.tMin <= 2) msg = evening ? "Ráno zima – bunda!" : "Ráno je zima – bunda!";
  else if (d.tMax >= 28) msg = "Vedro – pij hodně vody!";
  else if (d.code <= 1) msg = evening ? "Zítra bude hezky!" : "Dnes bude hezky!";
  else msg = evening ? "Dobrou noc!" : "Hezký den!";
  plate(c, 186, Canvas::textWidth(msg, FONT_SMALL) + 24.0f, 26);
  c.text(msg, 120, 190, FONT_SMALL, {255, 230, 150});
}

// ------------------------------------------------------------------ ISS

static void skyPoint(float az, float el, float &x, float &y) {
  float r = (90 - (el < 0 ? 0 : el)) / 90 * 74;
  x = 120 + r * sinf(az * DEG);
  y = 114 - r * cosf(az * DEG);
}

void RadarScreens::drawIss(Canvas &c, const IssInfo &iss, float t) {
  c.gradient({6, 10, 34}, {2, 3, 12});
  // twinkling stars
  uint32_t seed = 12345;
  for (int i = 0; i < 70; i++) {
    seed = seed * 1103515245 + 12345;
    float x = (seed >> 8) % 240;
    seed = seed * 1103515245 + 12345;
    float y = (seed >> 8) % 240;
    float tw = 0.5f + 0.5f * sinf(t * (1 + i % 3) + i);
    c.blend((int)x, (int)y, {255, 255, 255}, (uint32_t)(60 + 140 * tw));
  }
  Color grid = {90, 110, 180};
  c.circle(120, 114, 74, {20, 30, 70}, 120);
  c.ring(120, 114, 74, 1.5f, grid, 200);  // horizon
  c.ring(120, 114, 49, 1, grid, 80);      // 30 degrees
  c.ring(120, 114, 25, 1, grid, 80);      // 60 degrees
  c.text("S", 120, 42, FONT_TINY, grid);
  c.text("J", 120, 172, FONT_TINY, grid);
  c.text("V", 186, 107, FONT_TINY, grid);
  c.text("Z", 54, 107, FONT_TINY, grid);
  c.text("ISS", 120, 4, FONT_SMALL, {255, 255, 255});

  if (!iss.valid) {
    plate(c, 150, 190, 28);
    c.text(iss.status && *iss.status ? iss.status : "Počítám dráhu ISS…", 120, 155, FONT_TINY, {230, 220, 255});
    return;
  }
  if (iss.hasPass && iss.trackCount > 1) {
    float xy[ISS_TRACK_POINTS * 2];
    for (int i = 0; i < iss.trackCount; i++) skyPoint(iss.track[i][0], iss.track[i][1], xy[i * 2], xy[i * 2 + 1]);
    c.polyline(xy, iss.trackCount, 2.5f, {255, 210, 120}, 200);
    c.circle(xy[0], xy[1], 4, {120, 255, 160});
    // arrow at the end
    int n = iss.trackCount;
    float ex = xy[n * 2 - 2], ey = xy[n * 2 - 1], dx = ex - xy[n * 2 - 4], dy = ey - xy[n * 2 - 3];
    float len = sqrtf(dx * dx + dy * dy);
    if (len > 0.1f) {
      dx /= len, dy /= len;
      c.triangle(ex + dx * 7, ey + dy * 7, ex - dy * 5, ey + dx * 5, ex + dy * 5, ey - dx * 5, {255, 210, 120});
    }
  }
  if (iss.el > 0) {
    float x, y;
    skyPoint(iss.az, iss.el, x, y);
    float pulse = 0.6f + 0.4f * sinf(t * 5);
    c.glow(x, y, 16, 16, iss.sunlit ? Color{255, 255, 220} : Color{120, 140, 200}, (uint8_t)(160 * pulse));
    c.circle(x, y, 4.5f, iss.sunlit ? Color{255, 255, 255} : Color{150, 160, 200});
  }
  char line[64];
  if (iss.el > 0 && iss.sunlit) {
    snprintf(line, sizeof(line), "Teď! Dívej se na %s", bearingName(iss.az));
  } else if (iss.hasPass) {
    snprintf(line, sizeof(line), "%s", iss.when);
  } else {
    snprintf(line, sizeof(line), "Dnes nepřeletí viditelně");
  }
  FontId f = Canvas::textWidth(line, FONT_SMALL) > 150 ? FONT_TINY : FONT_SMALL;
  plate(c, 193, Canvas::textWidth(line, f) + 22.0f, 25);
  c.text(line, 120, f == FONT_SMALL ? 196 : 199, f, {255, 230, 150});
  if (iss.hasPass) {
    snprintf(line, sizeof(line), "%s → %s · výška %.0f°", bearingName(iss.azStart), bearingName(iss.azEnd), iss.maxEl);
  } else {
    snprintf(line, sizeof(line), "ISS je %.0f km daleko", iss.distKm);
  }
  c.text(line, 120, 24, FONT_TINY, {190, 200, 235});
}
