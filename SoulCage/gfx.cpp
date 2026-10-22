#include "gfx.h"
#include <math.h>

static inline float clamp01(float v) {
  return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

static inline float fmaxf3(float a, float b, float c) {
  return fmaxf(a, fmaxf(b, c));
}

Color mixColor(Color a, Color b, float t) {
  t = clamp01(t);
  return {(uint8_t)(a.r + (b.r - a.r) * t), (uint8_t)(a.g + (b.g - a.g) * t), (uint8_t)(a.b + (b.b - a.b) * t)};
}

Canvas::Canvas(uint16_t *buffer, int width, int height) : buf(buffer), w(width), h(height) {
  resetClip();
}

void Canvas::setClip(int x0, int y0, int x1, int y1) {
  cx0 = x0 < 0 ? 0 : x0;
  cy0 = y0 < 0 ? 0 : y0;
  cx1 = x1 > w ? w : x1;
  cy1 = y1 > h ? h : y1;
}

void Canvas::resetClip() {
  setClip(0, 0, w, h);
}

void Canvas::fill(Color c) {
  uint16_t v = ((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3);
  for (int i = 0; i < w * h; i++) {
    buf[i] = v;
  }
}

void Canvas::gradient(Color top, Color bottom) {
  // Ordered dithering hides the banding of 16-bit colour.
  static const uint8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  for (int y = 0; y < h; y++) {
    float t = (float)y / (h - 1);
    float r = (top.r + (bottom.r - top.r) * t) * (31.0f / 255) * 16;
    float g = (top.g + (bottom.g - top.g) * t) * (63.0f / 255) * 16;
    float b = (top.b + (bottom.b - top.b) * t) * (31.0f / 255) * 16;
    uint16_t *row = buf + y * w;
    for (int x = 0; x < w; x++) {
      int d = bayer[(x & 3) | ((y & 3) << 2)];
      int qr = ((int)r + d) >> 4, qg = ((int)g + d) >> 4, qb = ((int)b + d) >> 4;
      row[x] = ((qr > 31 ? 31 : qr) << 11) | ((qg > 63 ? 63 : qg) << 5) | (qb > 31 ? 31 : qb);
    }
  }
}

void Canvas::overlay(Color c, uint8_t alpha) {
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      blend(x, y, c, alpha);
    }
  }
}

void Canvas::rect(float x, float y, float rw, float rh, Color c, uint8_t a) {
  shade(x, y, x + rw, y + rh, c, a, [&](float px, float py) {
    float cx = fminf(px - x, x + rw - px), cy = fminf(py - y, y + rh - py);
    return clamp01(fminf(cx, cy) + 0.5f);
  });
}

void Canvas::roundRect(float x, float y, float rw, float rh, float radius, Color c, uint8_t a) {
  float hx = rw / 2, hy = rh / 2, mx = x + hx, my = y + hy;
  shade(x, y, x + rw, y + rh, c, a, [&](float px, float py) {
    float qx = fabsf(px - mx) - (hx - radius), qy = fabsf(py - my) - (hy - radius);
    float outside = sqrtf(fmaxf(qx, 0.0f) * fmaxf(qx, 0.0f) + fmaxf(qy, 0.0f) * fmaxf(qy, 0.0f));
    float d = outside + fminf(fmaxf(qx, qy), 0.0f) - radius;
    return clamp01(0.5f - d);
  });
}

void Canvas::ellipse(float cx, float cy, float rx, float ry, Color c, uint8_t a, float angleDeg) {
  if (rx <= 0 || ry <= 0) {
    return;
  }
  float r = fmaxf(rx, ry), minR = fminf(rx, ry);
  float an = angleDeg * 0.017453293f, ca = cosf(an), sa = sinf(an);
  shade(cx - r, cy - r, cx + r, cy + r, c, a, [&](float px, float py) {
    float dx = px - cx, dy = py - cy;
    float u = (dx * ca + dy * sa) / rx, v = (-dx * sa + dy * ca) / ry;
    float q = u * u + v * v;
    if (q > 2.5f) {
      return 0.0f;
    }
    return clamp01((1.0f - sqrtf(q)) * minR + 0.5f);
  });
}

void Canvas::glow(float cx, float cy, float rx, float ry, Color c, uint8_t a) {
  shade(cx - rx, cy - ry, cx + rx, cy + ry, c, a, [&](float px, float py) {
    float u = (px - cx) / rx, v = (py - cy) / ry;
    float q = 1.0f - (u * u + v * v);
    return q <= 0.0f ? 0.0f : q * q;
  });
}

static float segmentDistance(float px, float py, float x0, float y0, float x1, float y1) {
  float vx = x1 - x0, vy = y1 - y0, wx = px - x0, wy = py - y0;
  float len2 = vx * vx + vy * vy;
  float t = len2 > 0 ? clamp01((wx * vx + wy * vy) / len2) : 0.0f;
  float dx = wx - vx * t, dy = wy - vy * t;
  return sqrtf(dx * dx + dy * dy);
}

void Canvas::line(float x0, float y0, float x1, float y1, float width, Color c, uint8_t a) {
  float hw = width / 2;
  shade(fminf(x0, x1) - hw, fminf(y0, y1) - hw, fmaxf(x0, x1) + hw, fmaxf(y0, y1) + hw, c, a,
        [&](float px, float py) { return clamp01(hw - segmentDistance(px, py, x0, y0, x1, y1) + 0.5f); });
}

void Canvas::polyline(const float *xy, int points, float width, Color c, uint8_t a) {
  if (points < 2) {
    return;
  }
  float minX = xy[0], maxX = xy[0], minY = xy[1], maxY = xy[1];
  for (int i = 1; i < points; i++) {
    minX = fminf(minX, xy[i * 2]);
    maxX = fmaxf(maxX, xy[i * 2]);
    minY = fminf(minY, xy[i * 2 + 1]);
    maxY = fmaxf(maxY, xy[i * 2 + 1]);
  }
  float hw = width / 2;
  // One pass with the minimum distance, so joints are not blended twice.
  shade(minX - hw, minY - hw, maxX + hw, maxY + hw, c, a, [&](float px, float py) {
    float best = 1e9f;
    for (int i = 0; i + 1 < points; i++) {
      best = fminf(best, segmentDistance(px, py, xy[i * 2], xy[i * 2 + 1], xy[i * 2 + 2], xy[i * 2 + 3]));
    }
    return clamp01(hw - best + 0.5f);
  });
}

void Canvas::arc(float cx, float cy, float rx, float ry, float a0, float a1, float width, Color c, uint8_t a) {
  const int n = 16;
  float pts[(n + 1) * 2];
  for (int i = 0; i <= n; i++) {
    float an = (a0 + (a1 - a0) * i / n) * 0.017453293f;
    pts[i * 2] = cx + rx * cosf(an);
    pts[i * 2 + 1] = cy + ry * sinf(an);
  }
  polyline(pts, n + 1, width, c, a);
}

static float triangleCoverage(float px, float py, const float *v, float sign) {
  float d = 1e9f;
  for (int i = 0; i < 3; i++) {
    float x0 = v[i * 2], y0 = v[i * 2 + 1], x1 = v[(i + 1) % 3 * 2], y1 = v[(i + 1) % 3 * 2 + 1];
    float ex = x1 - x0, ey = y1 - y0, len = sqrtf(ex * ex + ey * ey);
    if (len <= 0) {
      return 0.0f;
    }
    d = fminf(d, sign * ((px - x0) * ey - (py - y0) * ex) / len);
  }
  return clamp01(d + 0.5f);
}

void Canvas::triangle(float x0, float y0, float x1, float y1, float x2, float y2, Color c, uint8_t a) {
  float v[6] = {x0, y0, x1, y1, x2, y2};
  float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  float sign = area > 0 ? -1.0f : 1.0f;
  shade(fminf(x0, fminf(x1, x2)), fminf(y0, fminf(y1, y2)), fmaxf(x0, fmaxf(x1, x2)), fmaxf(y0, fmaxf(y1, y2)), c, a,
        [&](float px, float py) { return triangleCoverage(px, py, v, sign); });
}

void Canvas::heart(float cx, float cy, float s, Color c, uint8_t a) {
  float r = s * 0.55f, lx = cx - s * 0.48f, rx = cx + s * 0.48f, ly = cy - s * 0.2f;
  float v[6] = {cx - s * 1.0f, cy - s * 0.02f, cx + s * 1.0f, cy - s * 0.02f, cx, cy + s * 1.05f};
  float area = (v[2] - v[0]) * (v[5] - v[1]) - (v[4] - v[0]) * (v[3] - v[1]);
  float sign = area > 0 ? -1.0f : 1.0f;
  shade(cx - s * 1.05f, cy - s * 0.8f, cx + s * 1.05f, cy + s * 1.1f, c, a, [&](float px, float py) {
    float d1 = r - sqrtf((px - lx) * (px - lx) + (py - ly) * (py - ly));
    float d2 = r - sqrtf((px - rx) * (px - rx) + (py - ly) * (py - ly));
    return fmaxf3(clamp01(d1 + 0.5f), clamp01(d2 + 0.5f), triangleCoverage(px, py, v, sign));
  });
}

void Canvas::star(float cx, float cy, float s, Color c, uint8_t a) {
  shade(cx - s, cy - s, cx + s, cy + s, c, a, [&](float px, float py) {
    float dx = fabsf(px - cx), dy = fabsf(py - cy);
    // Union of two thin diamonds.
    float d1 = s - (dx * 4.0f + dy), d2 = s - (dx + dy * 4.0f);
    return clamp01(fmaxf(d1, d2) * 0.5f + 0.5f);
  });
}

void Canvas::span(int y, float xl, float xr, Color c, uint8_t a) {
  int yy = (int)(y + oy);
  xl += ox;
  xr += ox;
  if (yy < cy0 || yy >= cy1 || xr <= xl) {
    return;
  }
  int x0 = (int)xl, x1 = (int)xr + 1;
  if (x0 < cx0) x0 = cx0;
  if (x1 >= cx1) x1 = cx1 - 1;
  for (int x = x0; x <= x1; x++) {
    float k = clamp01(fminf(x + 0.5f - xl, xr - (x + 0.5f)) + 0.5f);
    if (k > 0) {
      blend(x, yy, c, (uint32_t)(a * k));
    }
  }
}
