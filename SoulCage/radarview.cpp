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
  float best = 1e9f;
  for (int sy = 0; sy < RADAR_PX; sy++) {
    for (int sx = 0; sx < RADAR_PX; sx++) {
      int level = rainLevel(px[sy * RADAR_PX + sx]);
      float x = (sx + 0.5f) * 240 / RADAR_PX, y = (sy + 0.5f) * 240 / RADAR_PX;
      if (!level) continue;
      float dx = x - 120, dy = y - 120, d = sqrtf(dx * dx + dy * dy);
      if (d > RIM) continue;
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
  return info;
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

void RadarScreens::drawWeather(Canvas &c, const GeoView &view, const WeatherFrame *frames, int count,
                               const char *homeName, const char *status, float t) {
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
    }
  }
  drawMap(c, view, {120, 140, 185}, 210, {190, 195, 220}, 220, homeName);
  drawHome(c, homeName, t, {255, 110, 190});

  if (count == 0) {
    plate(c, 150, 170, 28);
    c.text(status && *status ? status : "Načítám radar…", 120, 155, FONT_SMALL, {230, 220, 255});
    c.text("Meteoradar", 120, 6, FONT_SMALL, {255, 255, 255});
    return;
  }
  const WeatherFrame &f = frames[shown];
  if (f.minutesAgo > 0) {
    snprintf(buf, sizeof(buf), "%s  -%d min", f.time, f.minutesAgo);
  } else {
    snprintf(buf, sizeof(buf), "%s  teď", f.time);
  }
  c.text(buf, 120, 6, FONT_SMALL, {255, 255, 255});

  // Rain summary from the newest frame (computed once per new frame).
  static const uint16_t *cachedFor = nullptr;
  static RainInfo info;
  if (cachedFor != frames[count - 1].pixels) {
    cachedFor = frames[count - 1].pixels;
    info = rainInfo(cachedFor, view);
  }
  const char *msg;
  if (!info.known || info.level == 0) {
    msg = "Bez srážek v okolí";
  } else if (info.atHome) {
    msg = info.level >= 4 ? "Bouřka u nás!" : info.level >= 3 ? "U nás silně prší" : "U nás prší";
  } else {
    snprintf(buf, sizeof(buf), "%s %.0f km %s", info.level >= 4 ? "Bouřka" : "Déšť", info.distanceKm,
             bearingName(info.bearingDeg));
    msg = buf;
  }
  Color mc = info.atHome || info.level >= 4 ? Color{255, 200, 120} : Color{220, 230, 255};
  plate(c, 182, Canvas::textWidth(msg, FONT_SMALL) + 22.0f, 24);
  c.text(msg, 120, 185, FONT_SMALL, mc);

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

void RadarScreens::drawPlanes(Canvas &c, const GeoView &view, const Plane *planes, int count, const char *homeName,
                              const char *status, float t) {
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

  if (count == 0) {
    plate(c, 150, 190, 28);
    c.text(status && *status ? status : "Žádná letadla v okolí", 120, 155, FONT_SMALL, {200, 255, 210});
    c.text("Letadla", 120, 22, FONT_SMALL, {200, 255, 210});
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
    if (k < 5) {
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
}
