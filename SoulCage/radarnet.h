#pragma once
// Downloads for the radar screens, in a background task:
//  - ČHMÚ precipitation composite (opendata.chmi.cz, PNG every 5 min), decoded into
//    small frames around home,
//  - aircraft around home from ADS-B (opendata.adsb.fi, fallback api.adsb.lol),
//  - current temperature at home (api.open-meteo.com).
// Data sources as in MeteoPlaneRadar by chiptron.cz (MIT).
#include <Arduino.h>
#include "radarview.h"

void radarNetInit();
// What is (or soon will be) on screen; data is only fetched while it is wanted.
void radarNetDemand(bool weather, bool planes);
// Home position or range changed: frames are re-decoded, aircraft re-fetched.
void radarNetSetView(const GeoView &view);

int radarWeatherFrames(WeatherFrame *out);  // up to RADAR_FRAMES, oldest first
int radarPlanes(Plane *out, int max);
// Text for an empty screen ("Bez internetu", "Načítám…", "Chyba ČHMÚ"...).
const char *radarStatus(bool weather);
bool radarRainSoon(RainInfo &info);  // rain summary of the newest frame, if any
// Temperature at home (Open-Meteo, every 10 min while wanted).
void radarNetDemandTemperature();
bool radarCurrent(CurrentWeather &out);
// Aircraft within the radar range for the app, nearest first:
// {"status":"…","planes":[{"hex","cs","type","alt","spd","km","dir"}]}
void radarPlanesJson(String &out);
