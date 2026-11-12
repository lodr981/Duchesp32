#include "font_data.h"
#include "gfx.h"

static const FontData &fontData(FontId id) {
  return id == FONT_BIG ? FONT_BIG_DATA : id == FONT_TINY ? FONT_TINY_DATA : FONT_SMALL_DATA;
}

// Decodes one UTF-8 character and advances the pointer.
static uint32_t nextCodepoint(const char *&p) {
  uint8_t c = *p++;
  if (c < 0x80) return c;
  int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
  uint32_t cp = c & (0x3F >> extra);
  for (int i = 0; i < extra && (*p & 0xC0) == 0x80; i++) {
    cp = (cp << 6) | (*p++ & 0x3F);
  }
  return cp;
}

static const FontGlyph *findGlyph(const FontData &f, uint32_t code) {
  int lo = 0, hi = f.count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (f.glyphs[mid].code == code) return &f.glyphs[mid];
    if (f.glyphs[mid].code < code) lo = mid + 1;
    else hi = mid - 1;
  }
  return nullptr;
}

int Canvas::textWidth(const char *s, FontId font) {
  const FontData &f = fontData(font);
  int w = 0;
  while (*s) {
    const FontGlyph *g = findGlyph(f, nextCodepoint(s));
    if (g) w += g->advance;
  }
  return w;
}

int Canvas::lineHeight(FontId font) {
  return fontData(font).lineHeight;
}

void Canvas::text(const char *s, float x, float y, FontId font, Color c, uint8_t a, TextAlign align) {
  const FontData &f = fontData(font);
  if (align != ALIGN_LEFT) {
    int w = textWidth(s, font);
    x -= align == ALIGN_CENTER ? w / 2.0f : w;
  }
  int penX = (int)(x + ox + 0.5f), baseY = (int)(y + oy + 0.5f) + f.ascent;
  while (*s) {
    const FontGlyph *g = findGlyph(f, nextCodepoint(s));
    if (!g) continue;
    int rowBytes = (g->w + 1) / 2;
    const uint8_t *bits = f.bitmap + g->offset;
    for (int gy = 0; gy < g->h; gy++) {
      for (int gx = 0; gx < g->w; gx++) {
        uint8_t byte = bits[gy * rowBytes + gx / 2];
        uint8_t v = gx & 1 ? byte & 0x0F : byte >> 4;
        if (v) {
          blend(penX + g->xoff + gx, baseY + g->yoff + gy, c, (uint32_t)a * v / 15);
        }
      }
    }
    penX += g->advance;
  }
}
