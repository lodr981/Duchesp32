#include "radarnet.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <math.h>
#include <new>
#include <time.h>
#include "certs.h"
#include "net.h"

// ČHMÚ composite: maximum reflectivity, transparent PNG for map overlays.
#define CHMU_INDEX_URL "https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/"
#define CHMU_PREFIX "pacz2gmaps3.z_max3d."
// Geographic bounds of the whole picture and of the data part (Mercator), from ČHMÚ.
#define CHMU_LON_LEFT 11.267f
#define CHMU_LON_RIGHT 20.770f
#define CHMU_LAT_TOP 52.167f
#define CHMU_LAT_BOTTOM 48.047f
#define CHMU_LON_DATA_RIGHT 19.624f
#define CHMU_LAT_DATA_TOP 51.458f
#define CHMU_MAX_PNG (96 * 1024)

#define WEATHER_PERIOD_MS (5 * 60 * 1000UL)
#define PLANES_PERIOD_MS 8000UL
#define DEMAND_KEEP_MS 15000UL       // keep fetching this long after the screen was shown
#define WEATHER_KEEP_MS (20 * 60 * 1000UL)  // weather stays fresh for rain alerts

static SemaphoreHandle_t lock;
static GeoView view, decodedView;
static volatile uint32_t weatherWantedAt, planesWantedAt;
static volatile bool viewChanged;

// Weather frames: RADAR_FRAMES shown + one spare being decoded.
static uint16_t *slots[RADAR_FRAMES + 1];
static int slotOf[RADAR_FRAMES];  // frame i -> slot
static int spare = RADAR_FRAMES;
static int frameCount;
static char frameName[RADAR_FRAMES][48];
static char frameTime[RADAR_FRAMES][6];
static uint8_t *pngRaw[RADAR_FRAMES];  // downloaded files, kept for re-decoding
static size_t pngSize[RADAR_FRAMES];
static uint32_t weatherFetchedAt;
static const char *weatherStatus = "Načítám radar…";

static Plane planes[RADAR_MAX_PLANES];
static int planeCount;
static uint32_t planesFetchedAt;
static const char *planesStatus = "Hledám letadla…";

static PNG *png;
static uint8_t *download;
static char *jsonBody;
static const size_t JSON_CAP = 192 * 1024;

static void *psram(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}

// ------------------------------------------------------------------ HTTP

// Verified TLS first; these public feeds fall back to an unverified connection
// if their certificate chain is not in our small root list.
static int httpGet(HTTPClient &http, NetworkClientSecure &client, const char *url, bool &insecureUsed) {
  for (int attempt = 0; attempt < 2; attempt++) {
    if (attempt == 0) client.setCACert(OTA_ROOT_CAS);
    else client.setInsecure();
    http.setConnectTimeout(8000);
    http.setTimeout(12000);
    http.useHTTP10(true);  // plain body, no chunked encoding
    http.setUserAgent("SoulCage-pendant");
    if (!http.begin(client, url)) return -1;
    int code = http.GET();
    if (code > 0) {
      insecureUsed = attempt == 1;
      return code;
    }
    http.end();
  }
  return -1;
}

// Reads the whole body into buf (cap bytes). Returns the length or -1.
static long readBody(HTTPClient &http, uint8_t *buf, size_t cap) {
  NetworkClient *s = http.getStreamPtr();
  long declared = http.getSize(), got = 0;
  uint32_t last = millis();
  while (http.connected() || s->available()) {
    int n = s->available();
    if (n > 0) {
      if (got + n > (long)cap) return -1;
      got += s->readBytes(buf + got, n);
      last = millis();
      if (declared > 0 && got >= declared) break;
    } else {
      if (millis() - last > 10000) return -1;
      delay(5);
    }
  }
  return declared > 0 && got < declared ? -1 : got;
}

// ------------------------------------------------------------------ ČHMÚ

static bool newerThan(const char *a, const char *b) {
  return strcmp(a + strlen(CHMU_PREFIX), b + strlen(CHMU_PREFIX)) > 0;
}

// Keeps the newest RADAR_FRAMES file names (ascending) while the index streams in.
static char topName[RADAR_FRAMES][48];
static int topCount;

static void topInsert(const char *name) {
  for (int i = 0; i < topCount; i++) {
    if (strcmp(topName[i], name) == 0) return;
  }
  if (topCount == RADAR_FRAMES) {
    if (!newerThan(name, topName[0])) return;
    memmove(topName[0], topName[1], sizeof(topName[0]) * (RADAR_FRAMES - 1));
    topCount--;
  }
  int p = topCount;
  while (p > 0 && newerThan(topName[p - 1], name)) {
    memcpy(topName[p], topName[p - 1], sizeof(topName[0]));
    p--;
  }
  strncpy(topName[p], name, sizeof(topName[0]) - 1);
  topName[p][sizeof(topName[0]) - 1] = 0;
  topCount++;
}

static bool scanIndex() {
  NetworkClientSecure client;
  HTTPClient http;
  bool insecure;
  topCount = 0;
  if (httpGet(http, client, CHMU_INDEX_URL, insecure) != 200) {
    http.end();
    return false;
  }
  NetworkClient *s = http.getStreamPtr();
  static char window[1024 + 65];
  size_t keep = 0;
  uint32_t last = millis();
  while (http.connected() || s->available()) {
    int n = s->available();
    if (n <= 0) {
      if (millis() - last > 10000) break;
      delay(5);
      continue;
    }
    last = millis();
    if (n > 1024) n = 1024;
    n = s->readBytes(window + keep, n);
    size_t len = keep + n;
    window[len] = 0;
    for (const char *p = window; (p = strstr(p, CHMU_PREFIX));) {
      const char *end = strstr(p, ".png");
      if (!end) break;
      size_t nameLen = end + 4 - p;
      if (nameLen < 47) {
        char name[48];
        memcpy(name, p, nameLen);
        name[nameLen] = 0;
        topInsert(name);
      }
      p = end + 4;
    }
    keep = len > 64 ? 64 : len;  // overlap, so a name split between reads is still found
    memmove(window, window + len - keep, keep);
  }
  http.end();
  return topCount > 0;
}

static bool downloadPng(const char *name, uint8_t **out, size_t *outSize) {
  char url[160];
  snprintf(url, sizeof(url), "%s%s", CHMU_INDEX_URL, name);
  NetworkClientSecure client;
  HTTPClient http;
  bool insecure;
  if (httpGet(http, client, url, insecure) != 200) {
    http.end();
    return false;
  }
  long got = readBody(http, download, CHMU_MAX_PNG);
  http.end();
  if (got < 8 || memcmp(download, "\x89PNG\r\n\x1a\n", 8) != 0) return false;
  uint8_t *copy = (uint8_t *)psram(got);
  if (!copy) return false;
  memcpy(copy, download, got);
  free(*out);
  *out = copy;
  *outSize = got;
  return true;
}

// UTC "YYYYMMDD.HHMM" in the name -> local "HH:MM".
static void localTime(const char *name, char *out) {
  const char *ts = name + strlen(CHMU_PREFIX);
  int Y, M, D, h, m;
  out[0] = 0;
  if (sscanf(ts, "%4d%2d%2d.%2d%2d", &Y, &M, &D, &h, &m) != 5) return;
  // days from civil (Howard Hinnant)
  Y -= M <= 2;
  int era = Y / 400, yoe = Y - era * 400, doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
  long days = era * 146097L + (yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
  time_t utc = days * 86400L + h * 3600 + m * 60;
  struct tm lt;
  localtime_r(&utc, &lt);
  snprintf(out, 6, "%02d:%02d", lt.tm_hour, lt.tm_min);
}

// --- PNG -> frame around home
static float mercY(float lat) {
  float r = lat * 0.017453293f;
  return logf(tanf(0.785398163f + r * 0.5f));
}

struct Decode {
  int imgW, imgH;
  int x1, y1, x2, y2;  // source crop
  uint16_t *crop;
  uint16_t line[2048];
};
static Decode *dec;

static float srcX(float lon) {
  return (lon - CHMU_LON_LEFT) * (dec->imgW - 1) / (CHMU_LON_RIGHT - CHMU_LON_LEFT);
}
static float srcY(float lat) {
  float yt = mercY(CHMU_LAT_TOP), yb = mercY(CHMU_LAT_BOTTOM);
  return (yt - mercY(lat)) * (dec->imgH - 1) / (yt - yb);
}

static int pngLine(PNGDRAW *d) {
  int y = d->y;
  if (y < dec->y1 || y > dec->y2 || d->iWidth > 2048) return 1;
  png->getLineAsRGB565(d, dec->line, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);  // transparent -> black
  int cw = dec->x2 - dec->x1 + 1;
  uint16_t *row = dec->crop + (y - dec->y1) * cw;
  for (int i = 0; i < cw; i++) {
    int x = dec->x1 + i;
    row[i] = x >= 0 && x < d->iWidth ? dec->line[x] : 0;
  }
  return 1;
}

static bool decodeFrame(const uint8_t *data, size_t size, const GeoView &v, uint16_t *outFrame) {
  if (png->openRAM((uint8_t *)data, size, pngLine) != PNG_SUCCESS) return false;
  dec->imgW = png->getWidth();
  dec->imgH = png->getHeight();
  // source crop covering the display square (+1 px margin)
  float latN, lonW, latS, lonE;
  v.unproject(0, 0, latN, lonW);
  v.unproject(240, 240, latS, lonE);
  dec->x1 = (int)floorf(srcX(lonW)) - 1;
  dec->x2 = (int)ceilf(srcX(lonE)) + 1;
  dec->y1 = (int)floorf(srcY(latN)) - 1;
  dec->y2 = (int)ceilf(srcY(latS)) + 1;
  int cw = dec->x2 - dec->x1 + 1, ch = dec->y2 - dec->y1 + 1;
  if (cw <= 0 || ch <= 0 || cw * ch > 400 * 400) {
    png->close();
    return false;
  }
  dec->crop = (uint16_t *)psram(cw * ch * 2);
  if (!dec->crop) {
    png->close();
    return false;
  }
  memset(dec->crop, 0, cw * ch * 2);
  png->decode(nullptr, 0);
  png->close();
  int dataRight = (int)srcX(CHMU_LON_DATA_RIGHT), dataTop = (int)srcY(CHMU_LAT_DATA_TOP);
  for (int y = 0; y < RADAR_PX; y++) {
    for (int x = 0; x < RADAR_PX; x++) {
      float lat, lon;
      v.unproject((x + 0.5f) * 240 / RADAR_PX, (y + 0.5f) * 240 / RADAR_PX, lat, lon);
      int sx = (int)lroundf(srcX(lon)), sy = (int)lroundf(srcY(lat));
      uint16_t c = 0;
      if (sx >= dec->x1 && sx <= dec->x2 && sy >= dec->y1 && sy <= dec->y2 && sx <= dataRight && sy >= dataTop) {
        c = dec->crop[(sy - dec->y1) * cw + (sx - dec->x1)];
        int r = c >> 11, g = (c >> 5) & 63, b = c & 31;
        if (r + g / 2 + b < 6) c = 0;  // (almost) black = no echo
      }
      outFrame[y * RADAR_PX + x] = c;
    }
  }
  free(dec->crop);
  dec->crop = nullptr;
  return true;
}

static void fetchWeather() {
  if (!scanIndex()) {
    weatherStatus = "ČHMÚ neodpovídá";
    return;
  }
  bool redecodeAll = viewChanged;
  viewChanged = false;
  GeoView v;
  xSemaphoreTake(lock, portMAX_DELAY);
  v = view;
  xSemaphoreGive(lock);
  // Frames we already have keep their file; only new names are downloaded.
  for (int i = 0; i < topCount; i++) {
    int have = -1;
    for (int k = 0; k < frameCount; k++) {
      if (strcmp(frameName[k], topName[i]) == 0) have = k;
    }
    if (have >= 0 && !redecodeAll) continue;
    uint8_t *raw = nullptr;
    size_t rawSize = 0;
    if (have >= 0) {  // re-decode the stored file for the new view
      raw = pngRaw[have];
      rawSize = pngSize[have];
      pngRaw[have] = nullptr;
    } else if (!downloadPng(topName[i], &raw, &rawSize)) {
      weatherStatus = "Chyba stahování radaru";
      free(raw);
      continue;
    }
    if (!decodeFrame(raw, rawSize, v, slots[spare])) {
      free(raw);
      continue;
    }
    // Insert in time order; the oldest frame drops out when full.
    xSemaphoreTake(lock, portMAX_DELAY);
    if (have >= 0) {
      int old = slotOf[have];
      slotOf[have] = spare;
      spare = old;
      pngRaw[have] = raw;
    } else {
      if (frameCount == RADAR_FRAMES) {
        free(pngRaw[0]);
        int dropped = slotOf[0];
        memmove(&slotOf[0], &slotOf[1], sizeof(int) * (RADAR_FRAMES - 1));
        memmove(frameName[0], frameName[1], sizeof(frameName[0]) * (RADAR_FRAMES - 1));
        memmove(frameTime[0], frameTime[1], sizeof(frameTime[0]) * (RADAR_FRAMES - 1));
        memmove(&pngRaw[0], &pngRaw[1], sizeof(pngRaw[0]) * (RADAR_FRAMES - 1));
        memmove(&pngSize[0], &pngSize[1], sizeof(pngSize[0]) * (RADAR_FRAMES - 1));
        frameCount--;
        int s = spare;
        spare = dropped;
        dropped = s;
        slotOf[frameCount] = dropped;
      } else {
        slotOf[frameCount] = spare;
        spare = RADAR_FRAMES;
        // find a free slot for the next spare
        for (int s = 0; s <= RADAR_FRAMES; s++) {
          bool used = false;
          for (int k = 0; k <= frameCount; k++) used |= slotOf[k] == s;
          if (!used) spare = s;
        }
      }
      strncpy(frameName[frameCount], topName[i], sizeof(frameName[0]) - 1);
      localTime(topName[i], frameTime[frameCount]);
      pngRaw[frameCount] = raw;
      pngSize[frameCount] = rawSize;
      frameCount++;
    }
    decodedView = v;
    xSemaphoreGive(lock);
  }
  weatherFetchedAt = millis();
  if (frameCount) weatherStatus = "";
}

// ------------------------------------------------------------------ ADS-B

static bool fetchPlanesFrom(const char *url) {
  NetworkClientSecure client;
  HTTPClient http;
  bool insecure;
  int code = httpGet(http, client, url, insecure);
  if (code != 200) {
    http.end();
    return false;
  }
  long len = readBody(http, (uint8_t *)jsonBody, JSON_CAP - 1);
  http.end();
  if (len < 2) return false;
  jsonBody[len] = 0;
  JsonDocument filter;
  for (const char *key : {"ac", "aircraft"}) {
    JsonObject f = filter[key].add<JsonObject>();
    f["flight"] = true;
    f["lat"] = true;
    f["lon"] = true;
    f["track"] = true;
    f["alt_baro"] = true;
    f["gs"] = true;
  }
  JsonDocument doc;
  if (deserializeJson(doc, jsonBody, len, DeserializationOption::Filter(filter))) return false;
  JsonArrayConst list = doc["ac"].as<JsonArrayConst>();
  if (list.isNull()) list = doc["aircraft"].as<JsonArrayConst>();
  if (list.isNull()) return false;
  Plane tmp[RADAR_MAX_PLANES];
  int n = 0;
  for (JsonObjectConst a : list) {
    if (n == RADAR_MAX_PLANES) break;
    if (!a["lat"].is<float>() || !a["lon"].is<float>()) continue;
    if (a["alt_baro"].is<const char *>()) continue;  // "ground"
    Plane &p = tmp[n];
    p.lat = a["lat"];
    p.lon = a["lon"];
    p.hasTrack = a["track"].is<float>();
    p.track = a["track"] | 0.0f;
    p.altM = (a["alt_baro"] | 0.0f) * 0.3048f;
    p.speedKmh = (a["gs"] | 0.0f) * 1.852f;
    const char *cs = a["flight"] | "";
    while (*cs == ' ') cs++;
    strncpy(p.callsign, cs, sizeof(p.callsign) - 1);
    p.callsign[sizeof(p.callsign) - 1] = 0;
    for (int i = strlen(p.callsign) - 1; i >= 0 && p.callsign[i] == ' '; i--) p.callsign[i] = 0;
    n++;
  }
  xSemaphoreTake(lock, portMAX_DELAY);
  memcpy(planes, tmp, sizeof(Plane) * n);
  planeCount = n;
  xSemaphoreGive(lock);
  return true;
}

static void fetchPlanes() {
  GeoView v;
  xSemaphoreTake(lock, portMAX_DELAY);
  v = view;
  xSemaphoreGive(lock);
  float nm = v.rangeKm / 1.852f;
  char url[160];
  snprintf(url, sizeof(url), "https://opendata.adsb.fi/api/v3/lat/%.5f/lon/%.5f/dist/%.0f", v.lat, v.lon, nm);
  bool ok = fetchPlanesFrom(url);
  if (!ok) {
    snprintf(url, sizeof(url), "https://api.adsb.lol/v2/point/%.5f/%.5f/%.0f", v.lat, v.lon, nm);
    ok = fetchPlanesFrom(url);
  }
  planesFetchedAt = millis();
  planesStatus = ok ? "Žádná letadla v okolí" : "Data letadel nedostupná";
}

// ------------------------------------------------------------------ task

static void radarTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(500));
    uint32_t now = millis();
    bool wantWeather = weatherWantedAt && now - weatherWantedAt < WEATHER_KEEP_MS;
    bool wantPlanes = planesWantedAt && now - planesWantedAt < DEMAND_KEEP_MS;
    if (!wantWeather && !wantPlanes) continue;
    if (!netOnline()) {
      weatherStatus = planesStatus = "Bez internetu – v aplikaci ulož Wi-Fi";
      continue;
    }
    if (wantPlanes && (!planesFetchedAt || now - planesFetchedAt > PLANES_PERIOD_MS)) {
      fetchPlanes();
    }
    if (wantWeather && (viewChanged || !weatherFetchedAt || millis() - weatherFetchedAt > WEATHER_PERIOD_MS)) {
      if (!frameCount) weatherStatus = "Stahuji radar ČHMÚ…";
      fetchWeather();
    }
  }
}

void radarNetInit() {
  lock = xSemaphoreCreateMutex();
  for (auto &s : slots) {
    s = (uint16_t *)psram(RADAR_PX * RADAR_PX * 2);
  }
  download = (uint8_t *)psram(CHMU_MAX_PNG);
  jsonBody = (char *)psram(JSON_CAP);
  png = new (psram(sizeof(PNG))) PNG();
  dec = (Decode *)psram(sizeof(Decode));
  // Local time for the frame labels (Central European with summer time).
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  xTaskCreatePinnedToCore(radarTask, "radar", 12288, nullptr, 1, nullptr, 0);
}

void radarNetDemand(bool weather, bool planes) {
  uint32_t now = millis();
  if (weather) weatherWantedAt = now ? now : 1;
  if (planes) planesWantedAt = now ? now : 1;
}

void radarNetSetView(const GeoView &v) {
  xSemaphoreTake(lock, portMAX_DELAY);
  bool moved = v.lat != view.lat || v.lon != view.lon || v.rangeKm != view.rangeKm;
  view = v;
  xSemaphoreGive(lock);
  if (moved) {
    viewChanged = true;
    planesFetchedAt = 0;
  }
}

int radarWeatherFrames(WeatherFrame *out) {
  xSemaphoreTake(lock, portMAX_DELAY);
  int n = frameCount;
  for (int i = 0; i < n; i++) {
    out[i].pixels = slots[slotOf[i]];
    out[i].minutesAgo = (n - 1 - i) * 5;
    memcpy(out[i].time, frameTime[i], sizeof(out[i].time));
  }
  xSemaphoreGive(lock);
  return n;
}

int radarPlanes(Plane *out, int max) {
  xSemaphoreTake(lock, portMAX_DELAY);
  int n = planeCount < max ? planeCount : max;
  memcpy(out, planes, sizeof(Plane) * n);
  xSemaphoreGive(lock);
  return n;
}

const char *radarStatus(bool weather) {
  return weather ? weatherStatus : planesStatus;
}

bool radarRainSoon(RainInfo &info) {
  WeatherFrame f[RADAR_FRAMES];
  int n = radarWeatherFrames(f);
  if (!n) return false;
  GeoView v;
  xSemaphoreTake(lock, portMAX_DELAY);
  v = decodedView;
  xSemaphoreGive(lock);
  info = rainInfo(f[n - 1].pixels, v);
  return info.known;
}
