#include "maze.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const float FIELD = 166;  // maze square inside the round display
static const Color WALL = {200, 176, 255};
static const Color CONFETTI[] = {{255, 122, 200}, {183, 155, 255}, {126, 232, 196}, {255, 220, 120}, {120, 200, 255}};

static float clampf(float v, float lo, float hi) {
  return v < lo ? lo : v > hi ? hi : v;
}

float MazeGame::random01() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (rng & 0xFFFFFF) / 16777216.0f;
}

// ------------------------------------------------------------------ levels

void MazeGame::generate(int level) {
  currentLevel = level < 1 ? 1 : level > MAZE_LEVELS ? MAZE_LEVELS : level;
  n = 5 + (currentLevel - 1) * 6 / (MAZE_LEVELS - 1);  // 5x5 .. 11x11
  cell = FIELD / n;
  ox = oy = 120 - FIELD / 2;
  rng = currentLevel * 2654435761u ^ 0x9E3779B9u;  // same maze every time for a level
  if (!rng) rng = 1;

  // Recursive backtracker (iterative).
  memset(walls, 3, sizeof(walls));
  bool visited[MAZE_MAX_N][MAZE_MAX_N] = {};
  int stack[MAZE_MAX_N * MAZE_MAX_N][2];
  int top = 0;
  stack[top][0] = 0, stack[top][1] = 0;
  top++;
  visited[0][0] = true;
  while (top > 0) {
    int x = stack[top - 1][0], y = stack[top - 1][1];
    int options[4], count = 0;
    if (x + 1 < n && !visited[y][x + 1]) options[count++] = 0;
    if (y + 1 < n && !visited[y + 1][x]) options[count++] = 1;
    if (x > 0 && !visited[y][x - 1]) options[count++] = 2;
    if (y > 0 && !visited[y - 1][x]) options[count++] = 3;
    if (!count) {
      top--;
      continue;
    }
    int dir = options[(int)(random01() * count) % count];
    int nx = x, ny = y;
    if (dir == 0) walls[y][x] &= ~1, nx++;
    if (dir == 1) walls[y][x] &= ~2, ny++;
    if (dir == 2) walls[y][x - 1] &= ~1, nx--;
    if (dir == 3) walls[y - 1][x] &= ~2, ny--;
    visited[ny][nx] = true;
    stack[top][0] = nx, stack[top][1] = ny;
    top++;
  }

  // Breadth-first distances: the exit is the cell farthest from the start.
  auto bfs = [&](int sx, int sy) {
    memset(dist, 0xFF, sizeof(dist));
    int queue[MAZE_MAX_N * MAZE_MAX_N][2], head = 0, tail = 0;
    dist[sy][sx] = 0;
    queue[tail][0] = sx, queue[tail][1] = sy;
    tail++;
    while (head < tail) {
      int x = queue[head][0], y = queue[head][1];
      head++;
      const int moves[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
      for (int d = 0; d < 4; d++) {
        int nx = x + moves[d][0], ny = y + moves[d][1];
        if (nx < 0 || ny < 0 || nx >= n || ny >= n || dist[ny][nx] != 0xFF) continue;
        bool open = d == 0 ? !(walls[y][x] & 1) : d == 1 ? !(walls[y][x] & 2) : d == 2 ? !(walls[y][x - 1] & 1)
                                                                                         : !(walls[y - 1][x] & 2);
        if (!open) continue;
        dist[ny][nx] = dist[y][x] + 1;
        queue[tail][0] = nx, queue[tail][1] = ny;
        tail++;
      }
    }
  };
  bfs(0, 0);
  exitX = exitY = 0;
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      if (dist[y][x] > dist[exitY][exitX]) exitX = x, exitY = y;
    }
  }
  bfs(exitX, exitY);  // from now on dist = steps to the exit
  pathLength = dist[0][0];

  // Holes off the solution path (level 8+).
  bool onPath[MAZE_MAX_N][MAZE_MAX_N] = {};
  for (int x = 0, y = 0;;) {
    onPath[y][x] = true;
    if (x == exitX && y == exitY) break;
    if (x + 1 < n && !(walls[y][x] & 1) && dist[y][x + 1] < dist[y][x]) x++;
    else if (y + 1 < n && !(walls[y][x] & 2) && dist[y + 1][x] < dist[y][x]) y++;
    else if (x > 0 && !(walls[y][x - 1] & 1) && dist[y][x - 1] < dist[y][x]) x--;
    else if (y > 0 && !(walls[y - 1][x] & 2) && dist[y - 1][x] < dist[y][x]) y--;
    else break;
  }
  holeCount = 0;
  int wanted = currentLevel >= 8 ? (currentLevel - 6 < 8 ? currentLevel - 6 : 8) : 0;
  for (int tries = 0; tries < 400 && holeCount < wanted; tries++) {
    int x = (int)(random01() * n) % n, y = (int)(random01() * n) % n;
    if (onPath[y][x] || x + y < 2) continue;
    bool taken = false;
    for (int i = 0; i < holeCount; i++) taken |= holes[i][0] == x && holes[i][1] == y;
    if (taken) continue;
    holes[holeCount][0] = x, holes[holeCount][1] = y;
    holeCount++;
  }
  lightRadius = currentLevel >= 11 ? 60 - (currentLevel - 11) * 4.5f : 0;
  radius = fmaxf(3.5f, cell * 0.27f);
}

void MazeGame::resetMarble() {
  bx = cellCenterX(0);
  by = cellCenterY(0);
  vx = vy = 0;
}

void MazeGame::start(int level, const MazeMapping &m) {
  map = m;
  generate(level);
  resetMarble();
  partCount = 0;
  stateTime = 0;
  playTime = 0;
  calSum[0] = calSum[1] = calSum[2] = 0;
  calSamples = 0;
  state = map.calibrated ? S_COUNTDOWN : S_CAL_FLAT;
}

void MazeGame::recalibrate() {
  map.calibrated = false;
  state = S_CAL_FLAT;
  stateTime = 0;
  calSum[0] = calSum[1] = calSum[2] = 0;
  calSamples = 0;
}

bool MazeGame::hint(float &x, float &y) const {
  int cx = (int)clampf((bx - ox) / cell, 0, n - 1), cy = (int)clampf((by - oy) / cell, 0, n - 1);
  int best = dist[cy][cx], tx = cx, ty = cy;
  if (cx + 1 < n && !(walls[cy][cx] & 1) && dist[cy][cx + 1] < best) best = dist[cy][cx + 1], tx = cx + 1, ty = cy;
  if (cy + 1 < n && !(walls[cy][cx] & 2) && dist[cy + 1][cx] < best) best = dist[cy + 1][cx], tx = cx, ty = cy + 1;
  if (cx > 0 && !(walls[cy][cx - 1] & 1) && dist[cy][cx - 1] < best) best = dist[cy][cx - 1], tx = cx - 1, ty = cy;
  if (cy > 0 && !(walls[cy - 1][cx] & 2) && dist[cy - 1][cx] < best) best = dist[cy - 1][cx], tx = cx, ty = cy - 1;
  x = cellCenterX(tx);
  y = cellCenterY(ty);
  return true;
}

// ------------------------------------------------------------------ physics

void MazeGame::collide() {
  float w = fmaxf(2.0f, cell * 0.12f) / 2;
  int cx = (int)floorf((bx - ox) / cell), cy = (int)floorf((by - oy) / cell);
  auto push = [&](float x0, float y0, float x1, float y1) {
    float qx = clampf(bx, x0, x1), qy = clampf(by, y0, y1);
    float dx = bx - qx, dy = by - qy, d = sqrtf(dx * dx + dy * dy);
    if (d >= radius) return;
    float nx, ny;
    if (d > 1e-4f) {
      nx = dx / d, ny = dy / d;
    } else {  // centre inside the wall: push out along the shortest side
      float l = bx - x0, r = x1 - bx, t = by - y0, b = y1 - by, m = fminf(fminf(l, r), fminf(t, b));
      nx = m == l ? -1 : m == r ? 1 : 0;
      ny = m == t ? -1 : m == b ? 1 : 0;
      d = 0;
    }
    bx += nx * (radius - d);
    by += ny * (radius - d);
    float vn = vx * nx + vy * ny;
    if (vn < 0) {
      vx -= 1.35f * vn * nx;  // restitution 0.35
      vy -= 1.35f * vn * ny;
    }
  };
  for (int y = cy - 1; y <= cy + 1; y++) {
    for (int x = cx - 1; x <= cx + 1; x++) {
      if (x < 0 || y < 0 || x >= n || y >= n) continue;
      float x0 = ox + x * cell, y0 = oy + y * cell, x1 = x0 + cell, y1 = y0 + cell;
      if (walls[y][x] & 1) push(x1 - w, y0 - w, x1 + w, y1 + w);
      if (walls[y][x] & 2) push(x0 - w, y1 - w, x1 + w, y1 + w);
      if (x == 0) push(x0 - w, y0 - w, x0 + w, y1 + w);
      if (y == 0) push(x0 - w, y0 - w, x1 + w, y0 + w);
    }
  }
  // Never leave the field.
  bx = clampf(bx, ox + radius, ox + FIELD - radius);
  by = clampf(by, oy + radius, oy + FIELD - radius);
}

void MazeGame::burst(float x, float y, int count, float speed) {
  for (int i = 0; i < count; i++) {
    if (partCount == (int)(sizeof(parts) / sizeof(parts[0]))) {
      memmove(parts, parts + 1, sizeof(Particle) * (partCount - 1));
      partCount--;
    }
    float a = random01() * 6.2832f, s = speed * (0.4f + 0.6f * random01());
    parts[partCount++] = {x, y, cosf(a) * s, sinf(a) * s - speed * 0.3f, 1.4f + random01(),
                          CONFETTI[(int)(random01() * 5) % 5]};
  }
}

MazeEvent MazeGame::update(float dt, float gx, float gy, float gz, Ghost &ghost, const GhostLook &look) {
  time += dt;
  stateTime += dt;
  float g[3] = {gx, gy, gz};
  int alive = 0;
  for (int i = 0; i < partCount; i++) {
    Particle p = parts[i];
    p.life -= dt;
    if (p.life <= 0) continue;
    p.vy += 180 * dt;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    parts[alive++] = p;
  }
  partCount = alive;

  switch (state) {
    case S_CAL_FLAT:
      if (stateTime > 0.8f) {
        for (int i = 0; i < 3; i++) calSum[i] += g[i];
        calSamples++;
      }
      if (stateTime > 2.4f && calSamples) {
        for (int i = 0; i < 3; i++) neutral[i] = calSum[i] / calSamples;
        state = S_CAL_RIGHT;
        stateTime = 0;
      }
      break;
    case S_CAL_RIGHT:
    case S_CAL_DOWN: {
      int best = -1;
      float bestDelta = 0;
      for (int i = 0; i < 3; i++) {
        if (state == S_CAL_DOWN && i == map.axisX) continue;
        float d = g[i] - neutral[i];
        if (fabsf(d) > fabsf(bestDelta)) bestDelta = d, best = i;
      }
      if (stateTime > 0.6f && best >= 0 && fabsf(bestDelta) > 0.35f) {
        if (state == S_CAL_RIGHT) {
          map.axisX = best;
          map.signX = bestDelta > 0 ? 1 : -1;
          state = S_CAL_DOWN;
          stateTime = 0;
        } else {
          map.axisY = best;
          map.signY = bestDelta > 0 ? 1 : -1;
          map.calibrated = true;
          state = S_COUNTDOWN;
          stateTime = 0;
          calSum[0] = calSum[1] = calSum[2] = 0;
          calSamples = 0;
          return MAZE_EV_CALIBRATED;
        }
      }
      break;
    }
    case S_COUNTDOWN:
      // The way the pendant is held now becomes "flat".
      if (stateTime > 0.3f) {
        for (int i = 0; i < 3; i++) calSum[i] += g[i];
        calSamples++;
      }
      if (stateTime > 2.6f) {
        if (calSamples) {
          for (int i = 0; i < 3; i++) neutral[i] = calSum[i] / calSamples;
        }
        state = S_PLAY;
        stateTime = 0;
        playTime = 0;
      }
      break;
    case S_PLAY: {
      playTime += dt;
      float tx = map.signX * (g[map.axisX] - neutral[map.axisX]);
      float ty = map.signY * (g[map.axisY] - neutral[map.axisY]);
      tx = fabsf(tx) < 0.03f ? 0 : clampf(tx, -0.8f, 0.8f);
      ty = fabsf(ty) < 0.03f ? 0 : clampf(ty, -0.8f, 0.8f);
      const int steps = 6;
      float h = dt / steps;
      for (int s = 0; s < steps; s++) {
        vx += tx * 720 * h;
        vy += ty * 720 * h;
        vx *= 1 - 1.6f * h;
        vy *= 1 - 1.6f * h;
        float sp = sqrtf(vx * vx + vy * vy);
        if (sp > 280) vx *= 280 / sp, vy *= 280 / sp;
        bx += vx * h;
        by += vy * h;
        collide();
      }
      for (int i = 0; i < holeCount; i++) {
        float hx = cellCenterX(holes[i][0]), hy = cellCenterY(holes[i][1]);
        if (hypotf(bx - hx, by - hy) < cell * 0.3f) {
          state = S_FALL;
          stateTime = 0;
          vx = hx, vy = hy;  // reused: where the marble falls to
        }
      }
      if (hypotf(bx - cellCenterX(exitX), by - cellCenterY(exitY)) < cell * 0.32f) {
        float par = pathLength * 0.9f + 2;
        stars = playTime <= par ? 3 : playTime <= par * 2 ? 2 : 1;
        stateTime = 0;
        partCount = 0;
        burst(120, 120, 60, 170);
        if (currentLevel >= MAZE_LEVELS) {
          state = S_ALL_DONE;
          ghost.trigger(GA_LEVELUP);
          return MAZE_EV_ALL_DONE;
        }
        state = S_WIN;
        ghost.trigger(GA_DANCE);
        return MAZE_EV_LEVEL_DONE;
      }
      break;
    }
    case S_FALL:
      playTime += dt;
      bx += (vx - bx) * clampf(dt * 10, 0, 1);
      by += (vy - by) * clampf(dt * 10, 0, 1);
      if (stateTime > 0.8f) {
        resetMarble();
        state = S_PLAY;
      }
      break;
    case S_WIN:
    case S_ALL_DONE: {
      GhostInput none;
      ghost.update(dt, none, look);
      float every = state == S_ALL_DONE ? 0.45f : 1.1f;
      if (fmodf(stateTime, every) < dt) {
        burst(50 + random01() * 140, 40 + random01() * 80, state == S_ALL_DONE ? 30 : 18, 130);
      }
      if (state == S_WIN && stateTime > 4.5f) {
        generate(currentLevel + 1);
        resetMarble();
        partCount = 0;
        state = S_COUNTDOWN;
        stateTime = 0;
        calSum[0] = calSum[1] = calSum[2] = 0;
        calSamples = 0;
      } else if (state == S_ALL_DONE && stateTime > 9) {
        state = S_OFF;
      }
      break;
    }
    default:
      break;
  }
  return MAZE_EV_NONE;
}

// ------------------------------------------------------------------ drawing

// Filled five-pointed star (anti-aliased by distance to the outline).
static void drawStar5(Canvas &c, float cx, float cy, float r, Color col, uint8_t a) {
  float pts[10][2];
  for (int i = 0; i < 10; i++) {
    float an = -1.5708f + i * 0.6283f, rr = i % 2 ? r * 0.45f : r;
    pts[i][0] = cx + rr * cosf(an);
    pts[i][1] = cy + rr * sinf(an);
  }
  c.shade(cx - r, cy - r, cx + r, cy + r, col, a, [&](float px, float py) {
    bool inside = false;
    float best = 1e9f;
    for (int i = 0, j = 9; i < 10; j = i++) {
      float xi = pts[i][0], yi = pts[i][1], xj = pts[j][0], yj = pts[j][1];
      if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi) inside = !inside;
      float ex = xj - xi, ey = yj - yi, l2 = ex * ex + ey * ey;
      float t = clampf(((px - xi) * ex + (py - yi) * ey) / l2, 0, 1);
      float dx = px - (xi + ex * t), dy = py - (yi + ey * t);
      best = fminf(best, dx * dx + dy * dy);
    }
    float d = sqrtf(best);
    return clampf(inside ? d + 0.5f : 0.5f - d, 0, 1);
  });
}

static void drawArrow(Canvas &c, float x0, float y0, float x1, float y1, Color col) {
  c.line(x0, y0, x1, y1, 7, col);
  float a = atan2f(y1 - y0, x1 - x0);
  c.triangle(x1 + cosf(a) * 14, y1 + sinf(a) * 14, x1 + cosf(a + 2.2f) * 14, y1 + sinf(a + 2.2f) * 14,
             x1 + cosf(a - 2.2f) * 14, y1 + sinf(a - 2.2f) * 14, col);
}

void MazeGame::drawParticles(Canvas &c) {
  for (int i = 0; i < partCount; i++) {
    const Particle &p = parts[i];
    c.circle(p.x, p.y, 2.2f, p.c, (uint8_t)(255 * clampf(p.life, 0, 1)));
  }
}

void MazeGame::drawMaze(Canvas &c) {
  c.gradient({34, 16, 58}, {10, 6, 20});
  c.roundRect(ox - 7, oy - 7, FIELD + 14, FIELD + 14, 14, {44, 26, 72});
  // portal
  float ex = cellCenterX(exitX), ey = cellCenterY(exitY), pulse = 0.5f + 0.5f * sinf(time * 4);
  c.glow(ex, ey, cell * 1.1f, cell * 1.1f, {255, 122, 200}, (uint8_t)(110 + 80 * pulse));
  c.circle(ex, ey, cell * 0.28f, {255, 160, 220});
  float spin = time * 200;
  c.arc(ex, ey, cell * 0.36f, cell * 0.36f, spin, spin + 220, fmaxf(1.5f, cell * 0.07f), {255, 255, 255}, 200);
  // holes
  for (int i = 0; i < holeCount; i++) {
    float hx = cellCenterX(holes[i][0]), hy = cellCenterY(holes[i][1]);
    c.circle(hx, hy, cell * 0.33f, {4, 2, 10});
    c.arc(hx, hy, cell * 0.33f, cell * 0.33f, 200, 340, 1.2f, {120, 90, 170}, 200);
  }
  // walls
  float w = fmaxf(2.0f, cell * 0.12f);
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      float x0 = ox + x * cell, y0 = oy + y * cell, x1 = x0 + cell, y1 = y0 + cell;
      if (walls[y][x] & 1) c.line(x1, y0, x1, y1, w, WALL);
      if (walls[y][x] & 2) c.line(x0, y1, x1, y1, w, WALL);
      if (x == 0) c.line(x0, y0, x0, y1, w, WALL);
      if (y == 0) c.line(x0, y0, x1, y0, w, WALL);
    }
  }
  // marble
  float r = radius;
  if (state == S_FALL) r *= clampf(1 - stateTime / 0.7f, 0.05f, 1);
  c.glow(bx, by, r * 2.4f, r * 2.4f, {170, 200, 255}, 90);
  c.circle(bx, by, r, {150, 165, 215});
  c.circle(bx - r * 0.18f, by - r * 0.18f, r * 0.78f, {225, 232, 255});
  c.circle(bx - r * 0.35f, by - r * 0.38f, r * 0.28f, {255, 255, 255});

  if (lightRadius > 0) {
    // Only the surroundings of the marble are lit, the portal still glows.
    uint16_t *px = c.pixels();
    float inner = lightRadius * 0.55f, span = lightRadius - inner;
    for (int y = 0; y < 240; y++) {
      for (int x = 0; x < 240; x++) {
        float d = sqrtf((x - bx) * (x - bx) + (y - by) * (y - by));
        if (d <= inner) continue;
        int keep = (int)(256 * (1 - clampf((d - inner) / span, 0, 1) * 0.94f));
        uint16_t v = px[y * 240 + x];
        int rr = ((v >> 11) & 31) * keep >> 8, gg = ((v >> 5) & 63) * keep >> 8, bb = (v & 31) * keep >> 8;
        px[y * 240 + x] = (rr << 11) | (gg << 5) | bb;
      }
    }
    c.glow(ex, ey, cell * 0.9f, cell * 0.9f, {255, 122, 200}, (uint8_t)(60 + 50 * pulse));
  }

  char hud[24];
  snprintf(hud, sizeof(hud), "Level %d/%d", currentLevel, MAZE_LEVELS);
  c.text(hud, 120, 8, FONT_SMALL, {230, 215, 255});
  if (state == S_PLAY || state == S_FALL) {
    int s = (int)playTime;
    snprintf(hud, sizeof(hud), "%d:%02d", s / 60, s % 60);
    c.text(hud, 120, 210, FONT_SMALL, {200, 180, 240});
  }
}

void MazeGame::render(Canvas &c, Ghost &ghost, const GhostLook &look) {
  char buf[40];
  switch (state) {
    case S_CAL_FLAT:
    case S_CAL_RIGHT:
    case S_CAL_DOWN: {
      c.gradient({58, 26, 96}, {16, 8, 30});
      c.text("Labyrint", 120, 30, FONT_BIG, {255, 255, 255});
      if (state == S_CAL_FLAT) {
        c.text("Drž přívěsek rovně", 120, 92, FONT_SMALL, {230, 215, 255});
        c.text("jako tácek…", 120, 114, FONT_SMALL, {230, 215, 255});
        float p = clampf(stateTime / 2.4f, 0, 1);
        c.arc(120, 172, 22, 22, -90, -90 + 360 * p, 5, {255, 122, 200});
      } else if (state == S_CAL_RIGHT) {
        c.text("Nakloň doprava", 120, 96, FONT_SMALL, {230, 215, 255});
        float wob = 6 * sinf(time * 6);
        drawArrow(c, 70 + wob, 160, 160 + wob, 160, {255, 122, 200});
      } else {
        c.text("Nakloň k sobě", 120, 92, FONT_SMALL, {230, 215, 255});
        float wob = 6 * sinf(time * 6);
        drawArrow(c, 120, 128 + wob, 120, 190 + wob, {255, 122, 200});
      }
      return;
    }
    case S_WIN:
    case S_ALL_DONE: {
      ghost.render(c, look);
      drawParticles(c);
      c.glow(120, 170, 120, 40, {30, 10, 50}, 160);
      if (state == S_WIN) {
        snprintf(buf, sizeof(buf), "Level %d hotovo!", currentLevel);
        c.text(buf, 120, 150, Canvas::textWidth(buf, FONT_BIG) < 200 ? FONT_BIG : FONT_SMALL, {255, 255, 255});
        for (int i = 0; i < 3; i++) {
          float pop = clampf((stateTime - 0.5f - i * 0.35f) * 4, 0, 1);
          bool earned = i < stars;
          drawStar5(c, 92 + i * 28, 200, 11 * (earned ? pop : 1), earned ? Color{255, 210, 80} : Color{90, 70, 120},
                    earned ? (uint8_t)(255 * pop) : 255);
        }
      } else {
        c.text("Mistryně", 120, 140, FONT_BIG, {255, 230, 140});
        c.text("labyrintů!", 120, 172, FONT_BIG, {255, 230, 140});
        c.text("Všech 15 levelů", 120, 208, FONT_SMALL, {255, 190, 230});
      }
      return;
    }
    default:
      break;
  }
  drawMaze(c);
  drawParticles(c);
  if (state == S_COUNTDOWN) {
    c.overlay({10, 4, 20}, 120);
    int k = 3 - (int)(stateTime / 0.7f);
    const char *label = k > 0 ? (k == 3 ? "3" : k == 2 ? "2" : "1") : "Jedeš!";
    c.text(label, 120, 100, FONT_BIG, {255, 255, 255});
    c.text("Drž přívěsek, jak chceš hrát", 120, 140, FONT_SMALL, {230, 215, 255});
  }
}
