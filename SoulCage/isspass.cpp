#include <math.h>
#include <stdio.h>
#include <string.h>
#include "isspass.h"

static double unixToJd(double t) { return t / 86400.0 + 2440587.5; }
static time_t jdToUnix(double jd) { return (time_t)llround((jd - 2440587.5) * 86400.0); }

bool issLoadTle(Sgp4 &sat, const char *text) {
  static char l1[130], l2[130];
  const char *a = strstr(text, "1 25544");
  const char *b = strstr(text, "2 25544");
  if (!a || !b) return false;
  int n1 = (int)strcspn(a, "\r\n"), n2 = (int)strcspn(b, "\r\n");
  if (n1 < 69 || n2 < 69 || n1 > 128 || n2 > 128) return false;
  memcpy(l1, a, n1);
  l1[n1] = 0;
  memcpy(l2, b, n2);
  l2[n2] = 0;
  char name[] = "ISS";
  return sat.init(name, l1, l2);
}

bool issFindPass(Sgp4 &sat, double lat, double lon, time_t now, IssInfo &info, time_t &start, time_t &end) {
  sat.site(lat, lon, 400);
  info.valid = true;
  info.hasPass = false;
  sat.initpredpoint(unixToJd((double)(now - 900)), 0.0);
  passinfo p;
  for (int i = 0; i < 16; i++) {
    if (!sat.nextpass(&p, 20)) break;
    time_t s = jdToUnix(p.jdstart), e = jdToUnix(p.jdstop);
    if (e < now) continue;
    if (s > now + 26 * 3600) break;
    if (p.maxelevation < 15) continue;
    // the visible part only: sunlit ISS in a dark sky, 10 degrees above the horizon
    float tr[ISS_TRACK_POINTS][2];
    int count = 0;
    time_t vs = 0, ve = 0;
    for (int k = 0; k < ISS_TRACK_POINTS; k++) {
      double jd = p.jdstart + (p.jdstop - p.jdstart) * k / (ISS_TRACK_POINTS - 1);
      sat.findsat(jd);
      if (sat.satVis >= 500 && sat.satEl >= 10) {
        if (!count) vs = jdToUnix(jd);
        ve = jdToUnix(jd);
        tr[count][0] = (float)sat.satAz;
        tr[count][1] = (float)sat.satEl;
        count++;
      }
    }
    if (count < 2) continue;
    info.hasPass = true;
    memcpy(info.track, tr, sizeof(tr));
    info.trackCount = count;
    info.azStart = tr[0][0];
    info.azEnd = tr[count - 1][0];
    info.maxEl = (float)p.maxelevation;
    start = vs;
    end = ve;
    return true;
  }
  return false;
}

static void formatWhen(time_t start, time_t now, char *out, size_t cap) {
  struct tm s, n;
  localtime_r(&start, &s);
  localtime_r(&now, &n);
  long mins = (long)((start - now) / 60);
  if (start <= now) snprintf(out, cap, "Právě teď!");
  else if (mins < 60) snprintf(out, cap, "Za %ld min (%d:%02d)", mins + 1, s.tm_hour, s.tm_min);
  else if (s.tm_yday == n.tm_yday) snprintf(out, cap, "Dnes v %d:%02d", s.tm_hour, s.tm_min);
  else snprintf(out, cap, "Zítra v %d:%02d", s.tm_hour, s.tm_min);
}

void issUpdateLive(Sgp4 &sat, time_t now, time_t start, time_t end, IssInfo &info) {
  sat.findsat(unixToJd((double)now));
  info.az = (float)sat.satAz;
  info.el = (float)sat.satEl;
  info.distKm = (float)sat.satDist;
  info.sunlit = sat.satVis >= 500;
  info.inPass = info.hasPass && now >= start - 60 && now <= end + 30;
  info.minutesTo = info.hasPass ? (int)((start - now) / 60) : 0;
  if (info.hasPass) formatWhen(start, now, info.when, sizeof(info.when));
}
