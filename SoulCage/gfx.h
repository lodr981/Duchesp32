#pragma once
// Small anti-aliased 2D renderer into an RGB565 buffer (native byte order).
// Platform independent: the same code runs on the pendant and in tools/preview.
#include <stdint.h>

struct Color {
  uint8_t r, g, b;
};

Color mixColor(Color a, Color b, float t);

class Canvas {
 public:
  Canvas(uint16_t *buffer, int width, int height);

  int width() const { return w; }
  int height() const { return h; }
  uint16_t *pixels() { return buf; }

  void setOffset(float x, float y) { ox = x; oy = y; }  // camera shake
  void setClip(int x0, int y0, int x1, int y1);
  void resetClip();

  void fill(Color c);
  void gradient(Color top, Color bottom);  // ignores offset, full screen
  void overlay(Color c, uint8_t alpha);     // whole screen tint (flash)

  void rect(float x, float y, float rw, float rh, Color c, uint8_t a = 255);
  void roundRect(float x, float y, float rw, float rh, float radius, Color c, uint8_t a = 255);
  void ellipse(float cx, float cy, float rx, float ry, Color c, uint8_t a = 255, float angleDeg = 0);
  void circle(float cx, float cy, float r, Color c, uint8_t a = 255) { ellipse(cx, cy, r, r, c, a); }
  void glow(float cx, float cy, float rx, float ry, Color c, uint8_t a);  // soft radial falloff
  void line(float x0, float y0, float x1, float y1, float width, Color c, uint8_t a = 255);
  void polyline(const float *xy, int points, float width, Color c, uint8_t a = 255);
  // Angles in degrees, 0 = right, clockwise (screen y grows down), like PIL.
  void arc(float cx, float cy, float rx, float ry, float a0, float a1, float width, Color c, uint8_t a = 255);
  void triangle(float x0, float y0, float x1, float y1, float x2, float y2, Color c, uint8_t a = 255);
  void heart(float cx, float cy, float size, Color c, uint8_t a = 255);
  void star(float cx, float cy, float size, Color c, uint8_t a = 255);  // 4-point sparkle
  // Horizontal anti-aliased span on pixel row y (used for the ghost tail).
  void span(int y, float xl, float xr, Color c, uint8_t a);

  void blend(int x, int y, Color c, uint32_t a);

  template <class Coverage>
  void shade(float fx0, float fy0, float fx1, float fy1, Color c, uint8_t a, Coverage cov);

 private:
  uint16_t *buf;
  int w, h;
  float ox = 0, oy = 0;
  int cx0, cy0, cx1, cy1;  // clip, inclusive-exclusive
};

inline void Canvas::blend(int x, int y, Color c, uint32_t a) {
  if (x < cx0 || y < cy0 || x >= cx1 || y >= cy1 || a == 0) {
    return;
  }
  uint16_t &p = buf[y * w + x];
  int sr = c.r >> 3, sg = c.g >> 2, sb = c.b >> 3;
  if (a >= 255) {
    p = (sr << 11) | (sg << 5) | sb;
    return;
  }
  int dr = (p >> 11) & 31, dg = (p >> 5) & 63, db = p & 31;
  dr += ((sr - dr) * (int)a) >> 8;
  dg += ((sg - dg) * (int)a) >> 8;
  db += ((sb - db) * (int)a) >> 8;
  p = (dr << 11) | (dg << 5) | db;
}

// Calls cov(px, py) -> 0..1 for every pixel centre in the box (screen coordinates).
template <class Coverage>
void Canvas::shade(float fx0, float fy0, float fx1, float fy1, Color c, uint8_t a, Coverage cov) {
  fx0 += ox;
  fx1 += ox;
  fy0 += oy;
  fy1 += oy;
  int x0 = (int)fx0 - 1, y0 = (int)fy0 - 1, x1 = (int)fx1 + 2, y1 = (int)fy1 + 2;
  if (x0 < cx0) x0 = cx0;
  if (y0 < cy0) y0 = cy0;
  if (x1 > cx1) x1 = cx1;
  if (y1 > cy1) y1 = cy1;
  for (int y = y0; y < y1; y++) {
    float py = y + 0.5f - oy;
    for (int x = x0; x < x1; x++) {
      float k = cov(x + 0.5f - ox, py);
      if (k <= 0.0f) {
        continue;
      }
      blend(x, y, c, k >= 1.0f ? a : (uint32_t)(a * k));
    }
  }
}
