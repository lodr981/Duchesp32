# Amelčin duch – přívěsek s pandím duchem (SoulCage)

Firmware pro přívěsek [SoulCage](https://github.com/vishalsoniindia/SoulCage---The-Digital-Trapped-Soul-Pendant)
od Vishala Soniho (MIT), rozšířený o:

- **Panda-duch** – roztomile strašidelný duch kreslený živě (25 snímků/s), reaguje na pohyb přívěsku
  a na povely z telefonu: mává, tancuje, dělá BAF!, schovává se, točí se, leká se, motá se mu hlava, spí.
  5 scén (hřbitov, bambusový les, strašidelný dům, sklenice, zima), 4 barvy očí, 3 stupně strašidelnosti.
- **Uvězněné duše** (původní fotografické animace) teď žijí: snímky jsou rozdělené na klipy (klid, bušení,
  klepání, přiběhnutí ke sklu) a duše reaguje na pohyb – při zatřesení buší do skla a sklo praská, při
  silném zatřesení sklo roztříští, obraz se rozpadne a duše uteče a pak se vrátí; 2× poklepání = klepe zpátky,
  naklonění = duše se nakloní za sklem, vzhůru nohama = visí hlavou dolů, v klidu usne a vybledne.
  Sama od sebe občas zabuší nebo zabliká světlo. Praskliny se po čase „zahojí“.
- **Mazlíček (Tamagotchi)** – duch má sytost, zábavu a energii. Krmí se (bambus, sušenka, dýňový koláč,
  bonbon), hraje si, hladí se, spí. Péčí roste přes 5 levelů (Dušínek → Malý strašák → Strašidlo →
  Velký bubák → Král duchů) a dostává mašli, tesáčky, klobouk a korunu. Nikdy neumře – jen je smutný nebo ospalý.
- **OTA aktualizace** – z GitHub Releases jedním tlačítkem v telefonu, nebo nahráním `.bin` souboru,
  s automatickým návratem k předchozí verzi, když nová nenaběhne.
- **Ovládání z iPhonu** – webová aplikace v Safari (přidáš si ji na plochu), REST API pro **Zkratky / Siri**.
- **Úspora baterie** – Wi-Fi se po nečinnosti sama vypne, měření baterie, varování, časovač uspání,
  ochrana proti hlubokému vybití.
- **Komprimované animace** – původních 15,3 MB snímků je bezeztrátově zmenšeno na 2,4 MB (zlib, dekomprese
  v ROM čipu). Díky tomu se do 16 MB flash vejdou dva OTA sloty.

Hardware: **Waveshare ESP32-S3-LCD-1.28** (kulatý GC9A01 240×240, ESP32-S3R2, 16 MB flash), Li-Po baterie, vypínač.

## Novinky 1.8 – Teplota a letadla v aplikaci 🌡️

- **Aktuální teplota** u vás (Open-Meteo, `api.open-meteo.com`, bez klíče, každých 10 minut): na meteoradaru
  nahoře „7 °C polojasno“, na radaru letadel dole. V aplikaci v kartě Radar teplota, obloha a vítr.
- **Letadla v aplikaci**: v kartě Radar rozbal „✈️ Letadla, která teď přívěsek vidí“ – seznam od nejbližšího
  (volací znak, typ, výška, rychlost, vzdálenost a směr). Klepnutím se letadlo otevře na živé mapě
  globe.adsb.fi, tlačítko **FR24** ho najde na Flightradar24.

## Novinky 1.7 – Meteoradar a radar letadel 🌦️✈️

Podle projektu [MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar) (chiptron.cz, MIT), překresleno
pro malý kulatý displej 240×240 bez dotyku. Střed: **Jenišovice u Jablonce** (jde změnit v aplikaci).

- **Meteoradar**: srážky z radarového kompozitu ČHMÚ (`opendata.chmi.cz`, PNG po 5 minutách, posledních 6 snímků
  jako animace) přes obrys hranic a místní obce. Dole souhrn: „Bez srážek v okolí“, „Déšť 9 km Z“, „U nás prší“,
  „Bouřka u nás!“. Dosah 25 / 50 / 100 km.
- **Radar letadel**: živá data ADS-B (`opendata.adsb.fi`, záloha `api.adsb.lol`), zelený radarový displej
  s kruhy vzdálenosti a otáčející se ručičkou, letadla jako šipky podle směru, barva podle výšky, volací znak
  a výška u nejbližších, dole nejbližší letadlo se vzdáleností a směrem.
- **Ovládání**: 1× ťuk přepíná meteoradar ↔ letadla, 2× ťuk nebo BOOT vrátí duši. V aplikaci karta Radar
  (Scény) s volbou dosahu a místa.
- **Upozornění na déšť**: když se déšť přiblíží na 15 km (nebo prší u vás), přívěsek sám na 30 s ukáže meteoradar
  (nejvýš jednou za 30 minut). Radar je i v rotaci Překvapení (jen když je přívěsek na internetu).
- Data se stahují na pozadí jen tehdy, když jsou potřeba. Přívěsek musí být připojený k Wi-Fi s internetem.
- Mapa: hranice Natural Earth (public domain) převzaté z MeteoPlaneRadar, obce doplněné ručně
  (`tools/make_map.py`). Knihovny PNGdec a ArduinoJson.

## Novinky 1.6 – Lovec světýlek 🏮

- Nová hra **Lovec světýlek** (hraje se kdykoli, nic se neschovává): nakláněním se vozí lucernička po skle,
  sbírají se modrá (1 bod) a zlatá (5 bodů) světýlka, combo až ×5, červení duchové berou srdíčka (3 životy),
  **ťuknutí = záblesk**, který je vyžene (dobíjí se 4 s). Každých 20 s nová, těžší vlna, občas srdíčko navíc.
  Rekord se ukládá, chycená světýlka se sbírají do **sklenice pro duši** (každých 100 = poděkování).
- Spuštění v aplikaci, nebo **bez telefonu: přívěsek položit displejem nahoru a 2× ťuknout**. Konec tlačítkem BOOT.
- **Plynulejší kulička** v labyrintu i lucernička: hry čtou rychlejší kanál akcelerometru, fyzika běží
  v pevných krocích 240×/s, statické části (stěny, pozadí) se kreslí jen jednou a tma se počítá bez odmocnin.
- Hra „Osvoboď duši“ (se schovávanými QR pečetěmi) zůstala, jen je v aplikaci až dole.

## Novinky 1.5 – Osvoboď duši 🔓

- **Jen uvězněná duše** (výchozí): panda a mazlíček jsou schovaní, přívěsek střídá muže a ženu za sklem a ti
  dělají pořád něco jiného (buší, klepou, houpají se v mlze, zhasne světlo, občas utečou). Vypíná se v aplikaci
  (Scény → Jen uvězněná duše).
- Odstraněna úvodní znělka YouTube/ElectroDonut z původního projektu. Po zapnutí se „Ahoj Amelko!“ ukáže přímo
  nad duší.
- **Ťuknutí**: citlivější rozpoznání, 1× ťuknout = duše zaklepe zpátky, 2× = zabuší a sklo praskne.
- **Hra „Osvoboď duši“** (telefon + QR kódy): v aplikaci se připraví hra se 3–10 pečetěmi a vytisknou se
  (`/hunt/print`, Safari → Sdílet → Tisk/PDF). Pečetě se schovají po bytě. Naskenování foťákem iPhonu
  (připojeného k Wi-Fi přívěsku) otevře stránku „Pečeť zlomena!“ s nápovědou k další pečeti (nápovědy píše rodič
  v aplikaci). Na přívěsku se kolem okraje odemyká zámek, duše buší a sklo praská; po poslední pečeti duše uteče
  na svobodu s ohňostrojem a poděkováním.

## Novinky 1.4 – Labyrint 🎮

- Hra **Labyrint s kuličkou**: nakláněním přívěsku se kutálí kulička do svítícího portálu (fyzika, odrazy od stěn).
- **15 levelů** (5×5 až 11×11), od levelu 8 **díry**, od levelu 11 **tma** (světlo jen kolem kuličky).
- Po každém levelu tančí panda, létají konfety a jsou **1–3 hvězdy** podle času, po 15. levelu ohňostroj a
  „Mistryně labyrintů!“. Postup a hvězdy se ukládají, každý level přidá duchovi zábavu a body péče.
- Při první hře přívěsek naučí ovládání (drž rovně → nakloň doprava → nakloň k sobě), takže směr vždy sedí,
  ať je senzor na desce otočený jakkoli.
- Spouští se v aplikaci (Můj duch → Labyrint, výběr levelu), API `/api/game?cmd=start&level=3`, `cmd=stop`,
  `cmd=calibrate`. Ukončí se i tlačítkem BOOT nebo po 2 minutách bez pohybu.
- Všech 15 levelů v CI dohraje „autopilot“ (`tools/preview/maze_preview.cpp`), takže každý jde projít.

## Novinky 1.3

- **QR kód po zapnutí** – po uvítání přívěsek ukáže QR kód své Wi-Fi (foťák iPhonu → Připojit, bez psaní hesla),
  a když se telefon připojí, druhý QR kód otevře aplikaci. Zatřesení nebo 2× ťuknutí ho přeskočí, vypnout jde v aplikaci.
  QR kódy generuje knihovna [qrcodegen](https://www.nayuki.io/page/qr-code-generator-library) (Project Nayuki, MIT).
- **Uvítání po zapnutí** – „Ahoj Amelko!“ s mávajícím duchem (text, podtitulek i jméno přívěsku jdou změnit v aplikaci).
- **Překvapení** – přívěsek sám chytře střídá všechny scény ducha i uvězněné duše (bez opakování, duše méně často
  v roztomilém režimu, když má mazlíček hlad, víc času dostane duch). Ruční výběr scény Překvapení vypne.
- **Hotspot**: běží pořád, když je Wi-Fi zapnutá (domácí síť se připojuje na pozadí jen kvůli internetu),
  nevypne se, dokud je k němu připojený telefon, nevyskakuje okno „přihlášení k síti“, nižší vysílací výkon.
  Jmenuje se podle přívěsku (výchozí **Amelčin duch**), adresa **http://192.168.4.1** nebo **http://amelka.local**.
- Nová aplikace pro puberťačku 💜: záložky Můj duch / Scény / Nastavení, vysvětlivky „?“ u každé části,
  rodičovská nastavení označená 👨‍👩‍👧.
- Hezčí texty na displeji s diakritikou (písmo Inter, SIL OFL).

## Ovládání bez telefonu (vypínačem)

| Gesto | Co udělá |
|---|---|
| zatřást | duch se lekne |
| dlouho a hodně zatřást | zamotá se mu hlava |
| 2× poklepat | pohladit (srdíčka, +zábava) |
| naklonit | duch odplave tím směrem a kouká tam |
| otočit vzhůru nohama | duch visí hlavou dolů a chichotá se |
| nechat ~2 min v klidu | usne a ztlumí displej, pohyb ho probudí |
| zapnout → vypnout → zapnout do 20 s | další scéna (u uvězněných duší další duše) |
| 3× rychle za sebou | **záchranný Wi-Fi hotspot** na 10 minut |
| tlačítko BOOT (pokud je v krabičce přístupné) | krátce = pohladit / další duše, 2 s = hotspot zap/vyp |

Pohybový senzor se při prvním klidném zavěšení sám zkalibruje; jde to i ručně v aplikaci (Duch → Pohybový senzor).

## Připojení iPhonu

1. Po zapnutí je Wi-Fi aktivní 5 minut (nastavitelné). Bez uložené sítě přívěsek vytvoří hotspot
   **Amelčin duch** (jméno jde změnit), heslo **soulcage**.
2. Na iPhonu se připoj k hotspotu a otevři **http://192.168.4.1** (často se otevře samo).
3. Přihlas se heslem zařízení (výchozí `soulcage`) a hned ho **změň** v části Zabezpečení.
4. V části Wi-Fi → Uložené sítě přidej domácí Wi-Fi a/nebo **osobní hotspot iPhonu**
   (kvůli aktualizacím z internetu). Přívěsek je pak i na **http://amelka.local**.
5. V Safari dej **Sdílet → Přidat na plochu** – vznikne ikona, která se chová jako aplikace.

Dokud máš webovou aplikaci otevřenou, Wi-Fi se nevypne. Malá azurová tečka nahoře na displeji = Wi-Fi zapnutá.

### Zkratky (Shortcuts) a Siri

Akce **Získat obsah URL**, metoda **POST**, hlavička `Authorization` = hodnota, kterou ukazuje webová
aplikace v sekci „Zkratky pro iPhone“ (`Basic …`).

| URL | Akce |
|---|---|
| `http://amelka.local/api/ghost?action=boo` | povel duchovi: `wave`, `dance`, `boo`, `hide`, `spin`, `laugh`, `love`, `scared`, `dizzy` |
| `http://amelka.local/api/pet?cmd=feed&food=bamboo` | nakrmit (`bamboo`, `cookie`, `pie`, `candy`) |
| `http://amelka.local/api/pet?cmd=play` | hrát si (dále `pet`, `sleep`, `wake`) |
| `http://amelka.local/api/scene?id=0` | scéna 0–4, `5` = uvězněné duše |
| `http://amelka.local/api/soul?step=next` | další uvězněná duše (`prev`, `random`) |
| `http://amelka.local/api/soul?action=bang` | uvězněná duše: `bang` (buší), `knock` (klepe), `escape` (útěk) |
| `http://amelka.local/api/soul?id=0` | konkrétní duše |
| `http://amelka.local/api/display` | displej zap/vyp (`?on=1` / `?on=0`) |
| `http://amelka.local/api/settings?brightness=40` | jas 5–100 (dále `eyeColor` 0–3, `spooky` 0–2, `frameDelay`, `sleepMinutes`, `wifiPolicy`, `wifiMinutes`, `intro`) |
| `http://amelka.local/api/sleep` | vypnout (hluboký spánek) |
| `GET http://amelka.local/api/status` | stav v JSON (mazlíček, baterie, verze, Wi-Fi…) |

Zkratku pojmenuj např. „Vyděs mě“ nebo „Nakrm pandu“ a spustíš ji hlasem přes Siri.

## Aktualizace (OTA)

- **Z internetu:** přívěsek musí být připojený k Wi-Fi s internetem (domácí síť nebo hotspot iPhonu).
  V aplikaci → Aktualizace → *Zkontrolovat* → *Nainstalovat*. Stahuje se `manifest.json` z posledního
  GitHub Release tohoto repa, firmware se ověří MD5 a zapíše do druhého slotu.
- **Ze souboru:** stáhni `soulcage.bin` z Releases do iPhonu (Soubory) a nahraj ho v aplikaci.
- **Pojistka:** když nová verze opakovaně padá, přívěsek se sám vrátí ke staré.

### Vydání nové verze

1. Zvyš `FW_VERSION` v `SoulCage/config.h` (např. `1.1.0`) a commitni.
2. Vytvoř tag a pushni: `git tag v1.1.0 && git push origin v1.1.0`.
   Nebo bez tagu: na GitHubu *Actions → Build firmware → Run workflow* a zaškrtni *Vytvořit Release*.
3. GitHub Actions zkompiluje firmware a vytvoří Release se soubory `soulcage.bin`, `soulcage-full.bin`
   a `manifest.json`. Přívěsky si aktualizaci najdou samy.

## První nahrání (jednou přes USB)

Kvůli nové tabulce oddílů je potřeba nahrát firmware jednou kabelem, pak už jen OTA.

**Nejjednodušší (bez instalace):** v Chrome/Edge na počítači otevři
[ESP Tool](https://espressif.github.io/esptool-js/), *Connect*, vyber port přívěsku, přidej soubor
`soulcage-full.bin` z [Releases](https://github.com/lodr981/Duchesp32/releases/latest) na adresu **0x0** a dej *Program*. (Když se nepřipojí: drž BOOT, stiskni RESET.)

**Arduino IDE:**
1. Deska *esp32 by Espressif* verze 3.3.x, knihovny **TFT_eSPI 2.5.43**, **PNGdec** a **ArduinoJson** (7.x).
2. Obsah `TFT_eSPI_Setup/Setup303_Waveshare_esp32s3_round_no_touch.h` zkopíruj do
   `Arduino/libraries/TFT_eSPI/User_Setup.h`.
3. Nástroje: *ESP32S3 Dev Module*, Flash Size **16MB**, Partition Scheme **Custom**,
   PSRAM **QSPI PSRAM**, USB CDC On Boot **Enabled**.
4. Otevři `SoulCage/SoulCage.ino` a nahraj.

**arduino-cli:**
```sh
arduino-cli compile --upload -p /dev/ttyACM0 \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,PSRAM=enabled" SoulCage
```

## Vlastní animace

Snímky jsou v `assets/<id>/frame_000.png…` (240×240). Seznam animací je v `tools/animations.json`
(zdroj může být i animovaný GIF). Po změně spusť:

```sh
pip install pillow
python3 tools/convert_animations.py   # vygeneruje SoulCage/anim_data.h
```

## Struktura

| Soubor | Obsah |
|---|---|
| `SoulCage/SoulCage.ino` | hlavní smyčka, režimy, gesta, mazlíček, časovač spánku |
| `gfx.*` | malý vyhlazený 2D renderer do RGB565 (běží i na PC) |
| `ghost.*` | panda-duch: kreslení, scény, akce a nálady |
| `pet.*` | logika mazlíčka (potřeby, krmení, levely) |
| `soul.*` | režisér uvězněných duší: klipy, reakce na pohyb, praskliny, šmouhy, glitch |
| `imu.*` | akcelerometr QMI8658, rozpoznání zatřesení, poklepání, náklonu a klidu |
| `display.*` | displej, jas (PWM), překryvné ikony |
| `animations.*`, `anim_data.h` | komprimované snímky a jejich dekomprese |
| `net.*`, `web_ui.h` | Wi-Fi (síť / hotspot / captive portal), mDNS, webové API a aplikace |
| `ota.*`, `certs.h` | OTA z GitHubu, nahrávání, rollback |
| `power.*` | baterie (GPIO1, dělič 1:3), hluboký spánek, počítání rychlých zapnutí |
| `settings.*` | nastavení v NVS |

## Náhled ducha na počítači

Kreslení ducha je nezávislé na hardwaru, takže jde vyzkoušet na PC:

```sh
cd tools/preview
gcc -O2 -c ../../SoulCage/qrcodegen.c -o qrcodegen.o
g++ -O2 -std=c++17 -I../../SoulCage preview.cpp ../../SoulCage/gfx.cpp ../../SoulCage/gfx_text.cpp \
  ../../SoulCage/greeting.cpp ../../SoulCage/ghost.cpp ../../SoulCage/pet.cpp qrcodegen.o -o preview
mkdir -p out && ./preview out 0 3 1 0 30   # scéna, level, strašidelnost, barva očí, sekundy
```

Vzniknou snímky `out/f0000.ppm…` (např. `ffmpeg -i out/f%04d.ppm duch.gif`).

Uvězněné duše (potřebuje zlib):

```sh
g++ -O2 -std=c++17 -Ishim -I../../SoulCage souls_preview.cpp ../../SoulCage/gfx.cpp ../../SoulCage/soul.cpp \
  ../../SoulCage/ghost.cpp ../../SoulCage/pet.cpp -lz -o souls_preview
mkdir -p out && ./souls_preview out 0 1 0 32   # duše 0/1, strašidelnost, barva, sekundy
```

## Licence

MIT – viz [LICENSE](LICENSE). Původní projekt a grafika © 2025 Vishal Soni.
