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
#include <WiFi.h>
#include <esp_system.h>
#include "certs.h"
#include "net.h"
#include "isspass.h"

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
static uint32_t weatherFetchedAt, weatherTriedAt;
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

// What happened to the last request of every source, for the app ("Stav stahování").
struct SourceDiag {
  const char *host;
  const char *name;
  int code;          // HTTP status, or a negative HTTPClient error
  char error[64];    // readable reason when it failed
  uint32_t at;       // millis of the last attempt
  uint32_t okAt;     // millis of the last success
  uint16_t ms;       // how long the last request took
  bool insecure;     // only worked without certificate check
};
static SourceDiag diag[] = {
    {"opendata.chmi.cz", "Radar ČHMÚ", 0, "", 0, 0, 0, false},
    {"opendata.adsb.fi", "Letadla adsb.fi", 0, "", 0, 0, 0, false},
    {"api.adsb.lol", "Letadla adsb.lol", 0, "", 0, 0, 0, false},
    {"api.open-meteo.com", "Teplota a předpověď", 0, "", 0, 0, 0, false},
    {"api.adsbdb.com", "Trasy letadel", 0, "", 0, 0, 0, false},
    {"celestrak.org", "Dráha ISS", 0, "", 0, 0, 0, false},
};

static SourceDiag *diagFor(const char *url) {
  for (SourceDiag &d : diag) {
    if (strstr(url, d.host)) return &d;
  }
  return nullptr;
}

// Verified TLS first; these public feeds fall back to an unverified connection
// if their certificate chain is not in our small root list.
static int httpGet(HTTPClient &http, NetworkClientSecure &client, const char *url, bool &insecureUsed) {
  SourceDiag *d = diagFor(url);
  uint32_t started = millis();
  if (d) d->at = started ? started : 1;
  // Name lookup first: a clear message when DNS is the problem.
  if (d) {
    IPAddress ip;
    if (!WiFi.hostByName(d->host, ip)) {
      d->code = -100;
      snprintf(d->error, sizeof(d->error), "DNS: %s nenalezen", d->host);
      d->ms = millis() - started;
      return -1;
    }
  }
  int code = -1;
  char tlsError[64] = "";
  for (int attempt = 0; attempt < 2; attempt++) {
    if (attempt == 0) client.setCACert(OTA_ROOT_CAS);
    else client.setInsecure();
    client.setHandshakeTimeout(15);  // seconds; the default is two minutes
    http.setConnectTimeout(8000);
    http.setTimeout(12000);
    http.useHTTP10(true);  // plain body, no chunked encoding
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setUserAgent("SoulCage-pendant/1.0 (ESP32)");
    if (!http.begin(client, url)) {
      code = -1;
      break;
    }
    code = http.GET();
    if (code > 0) {
      insecureUsed = attempt == 1;
      break;
    }
    client.lastError(tlsError, sizeof(tlsError));
    http.end();
  }
  if (d) {
    d->code = code;
    d->ms = millis() - started;
    d->insecure = code > 0 && insecureUsed;
    if (code == 200) {
      d->error[0] = 0;
    } else if (code > 0) {
      snprintf(d->error, sizeof(d->error), "server odpověděl HTTP %d", code);
    } else {
      String e = HTTPClient::errorToString(code);
      snprintf(d->error, sizeof(d->error), "%s%s%s", e.c_str(), tlsError[0] ? " · " : "", tlsError);
    }
  }
  return code;
}

// Marks the source of url as working (called after the data was parsed).
static void diagOk(const char *url) {
  SourceDiag *d = diagFor(url);
  if (d) {
    d->okAt = millis();
    if (!d->okAt) d->okAt = 1;
    d->error[0] = 0;
  }
}

static void diagFail(const char *url, const char *why) {
  SourceDiag *d = diagFor(url);
  if (d) snprintf(d->error, sizeof(d->error), "%s", why);
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
  if (topCount > 0) diagOk(CHMU_INDEX_URL);
  else diagFail(CHMU_INDEX_URL, "v seznamu ČHMÚ nejsou žádné snímky");
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
  if (got < 8 || memcmp(download, "\x89PNG\r\n\x1a\n", 8) != 0) {
    diagFail(url, got < 0 ? "snímek radaru se nestáhl celý" : "snímek radaru není PNG");
    return false;
  }
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

// Copies an identifier keeping only safe characters (it ends up in JSON and URLs).
static void copyId(char *out, size_t cap, const char *in) {
  size_t n = 0;
  for (; *in && n + 1 < cap; in++) {
    char ch = *in;
    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '~' || ch == '-') {
      out[n++] = ch;
    }
  }
  out[n] = 0;
}

static bool fetchPlanesFrom(const char *url, Plane *out, int &outCount) {
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
  if (len < 2) {
    diagFail(url, "odpověď se nestáhla celá");
    return false;
  }
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
    f["hex"] = true;
    f["t"] = true;
  }
  JsonDocument doc;
  DeserializationError je = deserializeJson(doc, jsonBody, len, DeserializationOption::Filter(filter));
  if (je) {
    char why[48];
    snprintf(why, sizeof(why), "chyba JSON: %s", je.c_str());
    diagFail(url, why);
    return false;
  }
  JsonArrayConst list = doc["ac"].as<JsonArrayConst>();
  if (list.isNull()) list = doc["aircraft"].as<JsonArrayConst>();
  if (list.isNull()) {
    diagFail(url, "v odpovědi chybí seznam letadel");
    return false;
  }
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
    copyId(p.hex, sizeof(p.hex), a["hex"] | "");
    copyId(p.type, sizeof(p.type), a["t"] | "");
    n++;
  }
  memcpy(out, tmp, sizeof(Plane) * n);
  outCount = n;
  diagOk(url);
  return true;
}

// Aircraft within radiusKm of home into out (adsb.fi, fallback adsb.lol).
static bool fetchPlanesAround(const GeoView &v, float radiusKm, Plane *out, int &n) {
  float nm = radiusKm / 1.852f;
  char url[160];
  snprintf(url, sizeof(url), "https://opendata.adsb.fi/api/v3/lat/%.5f/lon/%.5f/dist/%.0f", v.lat, v.lon, nm);
  if (fetchPlanesFrom(url, out, n)) return true;
  snprintf(url, sizeof(url), "https://api.adsb.lol/v2/point/%.5f/%.5f/%.0f", v.lat, v.lon, nm);
  return fetchPlanesFrom(url, out, n);
}

static float distanceKm(const GeoView &v, float lat, float lon) {
  float dx = (lon - v.lon) * 111.32f * cosf(v.lat * 0.017453293f), dy = (lat - v.lat) * 111.32f;
  return sqrtf(dx * dx + dy * dy);
}

static void checkOverhead(const GeoView &v, const Plane *list, int n);

static void fetchPlanes() {
  GeoView v;
  xSemaphoreTake(lock, portMAX_DELAY);
  v = view;
  xSemaphoreGive(lock);
  static Plane tmp[RADAR_MAX_PLANES];
  int n = 0;
  bool ok = fetchPlanesAround(v, v.rangeKm, tmp, n);
  if (ok) {
    xSemaphoreTake(lock, portMAX_DELAY);
    memcpy(planes, tmp, sizeof(Plane) * n);
    planeCount = n;
    xSemaphoreGive(lock);
    checkOverhead(v, tmp, n);
  }
  planesFetchedAt = millis();
  planesStatus = ok ? "Žádná letadla v okolí" : "Data letadel nedostupná";
}

// ------------------------------------------------------------------ aircraft right overhead

#define OVERHEAD_KM 3.0f
#define OVERHEAD_PERIOD_MS 20000UL
static volatile bool planeAlertOn, issAlertOn;
static OverheadAlert overhead;
static uint32_t overheadSeq, overheadFetchedAt;
static struct {
  char hex[8];
  uint32_t at;
} recentOverhead[8];

// English city names from the route database -> Czech for the common ones.
static const char *czechCity(const char *en) {
  static const char *const map[][2] = {
      {"Prague", "Praha"},       {"London", "Londýn"},      {"Vienna", "Vídeň"},        {"Munich", "Mnichov"},
      {"Warsaw", "Varšava"},     {"Rome", "Řím"},           {"Paris", "Paříž"},         {"Copenhagen", "Kodaň"},
      {"Brussels", "Brusel"},    {"Athens", "Atény"},       {"Lisbon", "Lisabon"},      {"Moscow", "Moskva"},
      {"Cologne", "Kolín n. R."}, {"Zurich", "Curych"},     {"Geneva", "Ženeva"},       {"Venice", "Benátky"},
      {"Milan", "Milán"},        {"Naples", "Neapol"},      {"Dresden", "Drážďany"},    {"Berlin", "Berlín"},
      {"Bucharest", "Bukurešť"}, {"Belgrade", "Bělehrad"},  {"Krakow", "Krakov"},       {"Kraków", "Krakov"},
      {"Wroclaw", "Vratislav"},  {"Wrocław", "Vratislav"},  {"Gdansk", "Gdaňsk"},       {"Gdańsk", "Gdaňsk"},
      {"Nuremberg", "Norimberk"}, {"Frankfurt am Main", "Frankfurt"}, {"Cairo", "Káhira"},
      {"Stockholm", "Stockholm"}, {"Helsinki", "Helsinky"}, {"Bratislava", "Bratislava"}, {"Budapest", "Budapešť"},
      {"Leipzig", "Lipsko"},     {"Hanover", "Hannover"},   {"Riga", "Riga"},           {"Vilnius", "Vilnius"},
      {"Tallinn", "Tallinn"},    {"Kyiv", "Kyjev"},         {"Kiev", "Kyjev"},          {"Sofia", "Sofie"},
      {"Istanbul", "Istanbul"},  {"Tel Aviv", "Tel Aviv"},  {"Tel Aviv-Yafo", "Tel Aviv"}, {"Heraklion", "Kréta"},
      {"Palma De Mallorca", "Mallorca"}, {"Palma", "Mallorca"}, {"New York", "New York"}, {"Beijing", "Peking"},
      {"Seoul", "Soul"},         {"Tokyo", "Tokio"},        {"Dubai", "Dubaj"},         {"Doha", "Dauhá"},
      {"Rhodes", "Rhodos"},      {"Corfu", "Korfu"},        {"Thessaloniki", "Soluň"},  {"Hurghada", "Hurghada"},
  };
  for (const auto &m : map) {
    if (strcmp(m[0], en) == 0) return m[1];
  }
  return en;
}

// Route and airline for a callsign from adsbdb.com (free, no key). Best effort.
static void lookupRoute(const char *callsign, OverheadAlert &a) {
  if (!callsign[0]) return;
  char url[96];
  snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/callsign/%s", callsign);
  NetworkClientSecure client;
  HTTPClient http;
  bool insecure;
  if (httpGet(http, client, url, insecure) != 200) {
    http.end();
    return;
  }
  long len = readBody(http, (uint8_t *)jsonBody, 16 * 1024);
  http.end();
  if (len < 2) return;
  jsonBody[len] = 0;
  JsonDocument doc;
  if (deserializeJson(doc, jsonBody, len)) return;
  diagOk(url);
  JsonObjectConst route = doc["response"]["flightroute"].as<JsonObjectConst>();
  if (route.isNull()) return;
  const char *from = route["origin"]["municipality"] | "";
  const char *to = route["destination"]["municipality"] | "";
  const char *airline = route["airline"]["name"] | "";
  snprintf(a.from, sizeof(a.from), "%s", czechCity(from));
  snprintf(a.to, sizeof(a.to), "%s", czechCity(to));
  snprintf(a.airline, sizeof(a.airline), "%s", airline);
}

static void checkOverhead(const GeoView &v, const Plane *list, int n) {
  if (!planeAlertOn) return;
  int best = -1;
  float bestKm = OVERHEAD_KM;
  for (int i = 0; i < n; i++) {
    if (list[i].altM < 300) continue;
    float km = distanceKm(v, list[i].lat, list[i].lon);
    if (km < bestKm) {
      bestKm = km;
      best = i;
    }
  }
  if (best < 0) return;
  const Plane &p = list[best];
  uint32_t now = millis();
  int slot = 0;
  for (int i = 0; i < 8; i++) {
    if (recentOverhead[i].at && strcmp(recentOverhead[i].hex, p.hex) == 0 && now - recentOverhead[i].at < 30 * 60000UL) {
      return;  // this one was announced already
    }
    if (recentOverhead[i].at < recentOverhead[slot].at) slot = i;
  }
  snprintf(recentOverhead[slot].hex, sizeof(recentOverhead[slot].hex), "%s", p.hex);
  recentOverhead[slot].at = now ? now : 1;
  OverheadAlert a;
  a.active = true;
  a.plane = p;
  a.distanceKm = bestKm;
  aircraftTypeName(p.type, a.typeName, sizeof(a.typeName));
  lookupRoute(p.callsign, a);
  xSemaphoreTake(lock, portMAX_DELAY);
  overhead = a;
  overheadSeq++;
  xSemaphoreGive(lock);
}

// Small, frequent look straight up while nothing else fetches aircraft.
static void pollOverhead() {
  overheadFetchedAt = millis();
  GeoView v;
  xSemaphoreTake(lock, portMAX_DELAY);
  v = view;
  xSemaphoreGive(lock);
  static Plane tmp[RADAR_MAX_PLANES];
  int n = 0;
  if (fetchPlanesAround(v, 6, tmp, n)) checkOverhead(v, tmp, n);
}

// ------------------------------------------------------------------ ISS

#define TLE_URL "https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=TLE"
static Sgp4 *sat;
static bool tleOk;
static uint32_t tleFetchedAt, tleTriedAt, issComputedAt, issLiveAt;
static volatile uint32_t issWantedAt;
static volatile bool issStale;
static IssInfo iss;
static time_t passStart, passEnd;
static const char *issStatus = "Počítám dráhu ISS…";

static char tleText[400];
static volatile bool tleNew;

static bool fetchTle() {
  tleTriedAt = millis();
  NetworkClientSecure client;
  HTTPClient http;
  bool insecure;
  if (httpGet(http, client, TLE_URL, insecure) != 200) {
    http.end();
    return false;
  }
  long len = readBody(http, (uint8_t *)jsonBody, 4096);
  http.end();
  if (len < 100) return false;
  jsonBody[len] = 0;
  if (!strstr(jsonBody, "1 25544") || !strstr(jsonBody, "2 25544")) {
    diagFail(TLE_URL, "v odpovědi není dráha ISS");
    return false;
  }
  diagOk(TLE_URL);
  snprintf(tleText, sizeof(tleText), "%s", jsonBody);
  tleNew = true;  // the ISS task loads it
  tleFetchedAt = millis();
  return true;
}

static void computePasses(time_t now) {
  GeoView v;
  xSemaphoreTake(lock, portMAX_DELAY);
  v = view;
  xSemaphoreGive(lock);
  issStale = false;
  IssInfo info;
  time_t s = 0, e = 0;
  issFindPass(*sat, v.lat, v.lon, now, info, s, e);
  xSemaphoreTake(lock, portMAX_DELAY);
  IssInfo old = iss;
  iss = info;
  if (old.valid) iss.az = old.az, iss.el = old.el, iss.distKm = old.distKm, iss.sunlit = old.sunlit;
  passStart = s;
  passEnd = e;
  xSemaphoreGive(lock);
  issComputedAt = millis();
}

static void updateIssLive(time_t now) {
  IssInfo info;
  xSemaphoreTake(lock, portMAX_DELAY);
  info = iss;
  xSemaphoreGive(lock);
  issUpdateLive(*sat, now, passStart, passEnd, info);
  xSemaphoreTake(lock, portMAX_DELAY);
  iss = info;
  xSemaphoreGive(lock);
  issLiveAt = millis();
}

// Network side: keep the orbit fresh (twice a day).
static bool tleDue() {
  uint32_t ms = millis();
  bool stale = !tleFetchedAt || ms - tleFetchedAt > 12 * 3600000UL;
  return stale && (!tleTriedAt || ms - tleTriedAt > (tleFetchedAt ? 30 * 60000UL : 2 * 60000UL));
}

// Compute side, in its own task: SGP4 is heavy on doubles and must never hold up downloads.
static void issTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    uint32_t ms = millis();
    bool wanted = issWantedAt && ms - issWantedAt < DEMAND_KEEP_MS;
    if (!wanted && !issAlertOn) continue;
    if (tleNew) {
      tleNew = false;
      if (issLoadTle(*sat, tleText)) {
        tleOk = true;
        issStale = true;
      } else {
        issStatus = "Dráha ISS se nedá přečíst";
      }
    }
    time_t now = time(nullptr);
    if (now < 1700000000) {
      issStatus = "Čekám na přesný čas…";
      continue;
    }
    if (!tleOk) {
      if (tleTriedAt && !tleFetchedAt) issStatus = "Dráha ISS nedostupná";
      continue;
    }
    if (!issComputedAt || issStale || ms - issComputedAt > 15 * 60000UL || (iss.hasPass && now > passEnd)) {
      computePasses(now);
    }
    bool soon = iss.hasPass && now >= passStart - 600 && now <= passEnd + 60;
    if (wanted || soon || millis() - issLiveAt > 60000) updateIssLive(now);
  }
}

// ------------------------------------------------------------------ Open-Meteo

static CurrentWeather current;
static Forecast forecast;
static uint32_t currentFetchedAt, currentTriedAt;
static volatile uint32_t tempWantedAt;
static volatile bool tempStale;  // home moved
#define CURRENT_PERIOD_MS (10 * 60 * 1000UL)
#define CURRENT_RETRY_MS (60 * 1000UL)

static void fetchCurrent() {
  GeoView v;
  xSemaphoreTake(lock, portMAX_DELAY);
  v = view;
  xSemaphoreGive(lock);
  char url[360];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,wind_speed_10m"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,"
           "precipitation_sum&timezone=Europe%%2FPrague&forecast_days=2",
           v.lat, v.lon);
  currentTriedAt = millis();
  tempStale = false;
  NetworkClientSecure client;
  HTTPClient http;
  bool insecure;
  if (httpGet(http, client, url, insecure) != 200) {
    http.end();
    return;
  }
  long len = readBody(http, (uint8_t *)jsonBody, JSON_CAP - 1);
  http.end();
  if (len < 2) {
    diagFail(url, "odpověď se nestáhla celá");
    return;
  }
  jsonBody[len] = 0;
  JsonDocument doc;
  if (deserializeJson(doc, jsonBody, len)) {
    diagFail(url, "chyba JSON");
    return;
  }
  JsonObjectConst cur = doc["current"].as<JsonObjectConst>();
  if (!cur["temperature_2m"].is<float>()) {
    diagFail(url, "v odpovědi chybí teplota");
    return;
  }
  diagOk(url);
  CurrentWeather w;
  w.valid = true;
  w.tempC = cur["temperature_2m"];
  w.code = cur["weather_code"] | -1;
  w.windKmh = cur["wind_speed_10m"] | 0.0f;
  Forecast f;
  JsonObjectConst daily = doc["daily"].as<JsonObjectConst>();
  if (!daily.isNull() && daily["temperature_2m_max"].size() >= 2) {
    f.valid = true;
    for (int i = 0; i < 2; i++) {
      f.day[i].code = daily["weather_code"][i] | -1;
      f.day[i].tMax = daily["temperature_2m_max"][i] | 0.0f;
      f.day[i].tMin = daily["temperature_2m_min"][i] | 0.0f;
      f.day[i].rainProb = daily["precipitation_probability_max"][i] | 0;
      f.day[i].rainMm = daily["precipitation_sum"][i] | 0.0f;
    }
  }
  xSemaphoreTake(lock, portMAX_DELAY);
  current = w;
  if (f.valid) forecast = f;
  xSemaphoreGive(lock);
  currentFetchedAt = millis();
}

// ------------------------------------------------------------------ task

// What the download task is doing right now (diagnostics).
static volatile const char *step = "start";
static volatile uint32_t stepAt, loopAt;
static TaskHandle_t netTaskHandle, issTaskHandle;

static void job(const char *what, void (*fn)()) {
  step = what;
  stepAt = millis();
  fn();
  step = "čekám";
  stepAt = millis();
}

static void fetchTleJob() {
  fetchTle();
}

// Download task: one request per round, the screen that is shown first, so a slow
// or failing source can never starve the others.
static void radarTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(300));
    uint32_t now = millis();
    loopAt = now ? now : 1;
    bool wantWeather = weatherWantedAt && now - weatherWantedAt < WEATHER_KEEP_MS;
    bool wantPlanes = planesWantedAt && now - planesWantedAt < DEMAND_KEEP_MS;
    bool wantTemp = tempWantedAt && now - tempWantedAt < WEATHER_KEEP_MS;
    bool wantIss = issWantedAt && now - issWantedAt < DEMAND_KEEP_MS;
    if (!wantWeather && !wantPlanes && !wantTemp && !wantIss && !planeAlertOn && !issAlertOn) continue;
    if (!netOnline()) {
      weatherStatus = planesStatus = "Bez internetu – v aplikaci ulož Wi-Fi";
      issStatus = "Bez internetu";
      step = "bez internetu";
      continue;
    }
    time_t clock = time(nullptr);
    struct tm lt;
    localtime_r(&clock, &lt);
    bool daytime = clock < 1700000000 || (lt.tm_hour >= 7 && lt.tm_hour < 21);  // alerts are shown 7-21 h
    if (wantPlanes && (!planesFetchedAt || now - planesFetchedAt > PLANES_PERIOD_MS)) {
      job("stahuji letadla", fetchPlanes);
    } else if (wantWeather && (viewChanged || !weatherFetchedAt || now - weatherFetchedAt > WEATHER_PERIOD_MS) &&
               (!weatherTriedAt || now - weatherTriedAt > 30000)) {
      weatherTriedAt = now;  // a failing ČHMÚ is retried every 30 s, not in a tight loop
      if (!frameCount) weatherStatus = "Stahuji radar ČHMÚ…";
      job("stahuji radar ČHMÚ", fetchWeather);
    } else if (wantTemp && (!currentFetchedAt || tempStale || now - currentFetchedAt > CURRENT_PERIOD_MS) &&
               (!currentTriedAt || tempStale || now - currentTriedAt > CURRENT_RETRY_MS)) {
      job("stahuji teplotu", fetchCurrent);
    } else if (!wantPlanes && planeAlertOn && daytime &&
               (!overheadFetchedAt || now - overheadFetchedAt > OVERHEAD_PERIOD_MS)) {
      job("hlídám letadla nad námi", pollOverhead);
    } else if ((wantIss || issAlertOn) && tleDue()) {
      job("stahuji dráhu ISS", fetchTleJob);
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
  sat = new Sgp4();
  xTaskCreatePinnedToCore(radarTask, "radar", 20480, nullptr, 1, &netTaskHandle, 0);
  xTaskCreatePinnedToCore(issTask, "iss", 16384, nullptr, 0, &issTaskHandle, 1);
}

void radarNetDemand(bool weather, bool planes) {
  uint32_t now = millis();
  if (weather || planes) tempWantedAt = now ? now : 1;
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
    tempStale = true;
    issStale = true;
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

void radarNetDemandTemperature() {
  uint32_t now = millis();
  tempWantedAt = now ? now : 1;
}

bool radarCurrent(CurrentWeather &out) {
  xSemaphoreTake(lock, portMAX_DELAY);
  out = current;
  xSemaphoreGive(lock);
  // older than an hour: better nothing than a stale number
  if (out.valid && millis() - currentFetchedAt > 60 * 60 * 1000UL) out.valid = false;
  return out.valid;
}

void radarPlanesJson(String &out) {
  static Plane list[RADAR_MAX_PLANES];
  GeoView v;
  xSemaphoreTake(lock, portMAX_DELAY);
  v = view;
  int n = planeCount;
  memcpy(list, planes, sizeof(Plane) * n);
  xSemaphoreGive(lock);
  float km[RADAR_MAX_PLANES];
  int order[RADAR_MAX_PLANES], m = 0;
  for (int i = 0; i < n; i++) {
    float dx = (list[i].lon - v.lon) * 111.32f * cosf(v.lat * 0.017453293f), dy = (list[i].lat - v.lat) * 111.32f;
    km[i] = sqrtf(dx * dx + dy * dy);
    if (km[i] <= v.rangeKm) order[m++] = i;  // only what fits on the pendant
  }
  for (int i = 1; i < m; i++) {
    for (int j = i; j > 0 && km[order[j]] < km[order[j - 1]]; j--) {
      int t = order[j];
      order[j] = order[j - 1];
      order[j - 1] = t;
    }
  }
  out = "{\"status\":\"";
  out += planesFetchedAt ? planesStatus : "Hledám letadla…";
  out += "\",\"planes\":[";
  char buf[200];
  for (int k = 0; k < m; k++) {
    const Plane &p = list[order[k]];
    float dx = (p.lon - v.lon) * 111.32f * cosf(v.lat * 0.017453293f), dy = (p.lat - v.lat) * 111.32f;
    float bearing = fmodf(atan2f(dx, dy) * 57.29578f + 360, 360);
    snprintf(buf, sizeof(buf),
             "%s{\"hex\":\"%s\",\"cs\":\"%s\",\"type\":\"%s\",\"alt\":%.0f,\"spd\":%.0f,\"km\":%.1f,\"dir\":\"%s\"}",
             k ? "," : "", p.hex, p.callsign, p.type, p.altM, p.speedKmh, km[order[k]], bearingName(bearing));
    out += buf;
  }
  out += "]}";
}

bool radarForecast(Forecast &out) {
  xSemaphoreTake(lock, portMAX_DELAY);
  out = forecast;
  xSemaphoreGive(lock);
  return out.valid;
}

void radarNetSetAlerts(bool planesOverhead, bool issPasses) {
  planeAlertOn = planesOverhead;
  issAlertOn = issPasses;
}

uint32_t radarOverhead(OverheadAlert &out) {
  xSemaphoreTake(lock, portMAX_DELAY);
  out = overhead;
  uint32_t seq = overheadSeq;
  xSemaphoreGive(lock);
  return seq;
}

void radarNetDemandIss() {
  uint32_t now = millis();
  issWantedAt = now ? now : 1;
}

bool radarIss(IssInfo &out) {
  xSemaphoreTake(lock, portMAX_DELAY);
  out = iss;
  xSemaphoreGive(lock);
  if (!out.valid) out.status = issStatus;
  return out.valid;
}

void radarNetDiagJson(String &out) {
  uint32_t now = millis();
  char head[300];
  snprintf(head, sizeof(head),
           "{\"step\":\"%s\",\"stepFor\":%lu,\"loopAgo\":%ld,\"stackNet\":%u,\"stackIss\":%u,\"uptime\":%lu,"
           "\"reset\":%d,\"sources\":[",
           (const char *)step, (unsigned long)((now - stepAt) / 1000), loopAt ? (long)((now - loopAt) / 1000) : -1L,
           netTaskHandle ? (unsigned)uxTaskGetStackHighWaterMark(netTaskHandle) : 0,
           issTaskHandle ? (unsigned)uxTaskGetStackHighWaterMark(issTaskHandle) : 0, (unsigned long)(now / 1000),
           (int)esp_reset_reason());
  out = head;
  bool first = true;
  for (const SourceDiag &d : diag) {
    if (!d.at) continue;
    char buf[200];
    snprintf(buf, sizeof(buf), "%s{\"name\":\"%s\",\"ok\":%s,\"code\":%d,\"ago\":%lu,\"okAgo\":%ld,\"ms\":%u,\"error\":\"",
             first ? "" : ",", d.name, d.okAt && d.okAt >= d.at - 1 && !d.error[0] ? "true" : "false", d.code,
             (unsigned long)((now - d.at) / 1000), d.okAt ? (long)((now - d.okAt) / 1000) : -1L, (unsigned)d.ms);
    out += buf;
    for (const char *c = d.error; *c; c++) {
      if (*c == '"' || *c == '\\') out += '\\';
      if ((uint8_t)*c >= 32) out += *c;
    }
    out += "\"}";
    first = false;
  }
  out += "]}";
}

void radarNetRetry() {
  uint32_t now = millis();
  if (!now) now = 1;
  weatherFetchedAt = weatherTriedAt = planesFetchedAt = currentFetchedAt = currentTriedAt = overheadFetchedAt = 0;
  tleTriedAt = 0;
  issComputedAt = 0;
  weatherWantedAt = planesWantedAt = tempWantedAt = issWantedAt = now;
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
