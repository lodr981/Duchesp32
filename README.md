# SoulCage – digitální přívěsek s uvězněnou duší

Firmware pro přívěsek [SoulCage](https://github.com/vishalsoniindia/SoulCage---The-Digital-Trapped-Soul-Pendant)
od Vishala Soniho (MIT), rozšířený o:

- **OTA aktualizace** – z GitHub Releases jedním tlačítkem v telefonu, nebo nahráním `.bin` souboru,
  s automatickým návratem k předchozí verzi, když nová nenaběhne.
- **Ovládání z iPhonu** – webová aplikace v Safari (přidáš si ji na plochu), REST API pro **Zkratky / Siri**.
- **Úspora baterie** – Wi-Fi se po nečinnosti sama vypne, měření baterie, varování, časovač uspání,
  ochrana proti hlubokému vybití.
- **Komprimované animace** – původních 15,3 MB snímků je bezeztrátově zmenšeno na 2,4 MB (zlib, dekomprese
  v ROM čipu). Díky tomu se do 16 MB flash vejdou dva OTA sloty.

Hardware: **Waveshare ESP32-S3-LCD-1.28** (kulatý GC9A01 240×240, ESP32-S3R2, 16 MB flash), Li-Po baterie, vypínač.

## Ovládání bez telefonu (vypínačem)

| Gesto | Co udělá |
|---|---|
| zapnout | přehraje úvod a pak vybranou duši |
| zapnout → vypnout → zapnout do 20 s | další duše (jako v originále) |
| 3× rychle za sebou | **záchranný Wi-Fi hotspot** na 10 minut (duše zůstane) |
| tlačítko BOOT (pokud je v krabičce přístupné) | krátce = další duše, 2 s = hotspot zap/vyp |

## Připojení iPhonu

1. Po zapnutí je Wi-Fi aktivní 5 minut (nastavitelné). Bez uložené sítě přívěsek vytvoří hotspot
   **SoulCage-xxxx**, heslo **soulcage**.
2. Na iPhonu se připoj k hotspotu a otevři **http://192.168.4.1** (často se otevře samo).
3. Přihlas se heslem zařízení (výchozí `soulcage`) a hned ho **změň** v části Zabezpečení.
4. V části Wi-Fi → Uložené sítě přidej domácí Wi-Fi a/nebo **osobní hotspot iPhonu**
   (pak přívěsek ovládáš i venku). Od té doby je na **http://soulcage.local**.
5. V Safari dej **Sdílet → Přidat na plochu** – vznikne ikona, která se chová jako aplikace.

Dokud máš webovou aplikaci otevřenou, Wi-Fi se nevypne. Malá azurová tečka nahoře na displeji = Wi-Fi zapnutá.

### Zkratky (Shortcuts) a Siri

Akce **Získat obsah URL**, metoda **POST**, hlavička `Authorization` = hodnota, kterou ukazuje webová
aplikace v sekci „Zkratky pro iPhone“ (`Basic …`).

| URL | Akce |
|---|---|
| `http://soulcage.local/api/soul?step=next` | další duše (`prev`, `random`) |
| `http://soulcage.local/api/soul?id=0` | konkrétní duše |
| `http://soulcage.local/api/display` | displej zap/vyp (`?on=1` / `?on=0`) |
| `http://soulcage.local/api/settings?brightness=40` | jas 5–100 (dále `frameDelay`, `sleepMinutes`, `wifiPolicy`, `wifiMinutes`, `intro`) |
| `http://soulcage.local/api/sleep` | uspat |
| `GET http://soulcage.local/api/status` | stav v JSON (baterie, verze, Wi-Fi…) |

Zkratku pojmenuj např. „Probuď duši“ a spustíš ji hlasem přes Siri.

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
1. Deska *esp32 by Espressif* verze 3.3.x, knihovna **TFT_eSPI 2.5.43**.
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
| `SoulCage/SoulCage.ino` | hlavní smyčka, gesta, časovač spánku |
| `display.*` | displej, jas (PWM), překryvné ikony |
| `animations.*`, `anim_data.h` | komprimované snímky a jejich dekomprese |
| `net.*`, `web_ui.h` | Wi-Fi (síť / hotspot / captive portal), mDNS, webové API a aplikace |
| `ota.*`, `certs.h` | OTA z GitHubu, nahrávání, rollback |
| `power.*` | baterie (GPIO1, dělič 1:3), hluboký spánek, počítání rychlých zapnutí |
| `settings.*` | nastavení v NVS |

## Licence

MIT – viz [LICENSE](LICENSE). Původní projekt a grafika © 2025 Vishal Soni.
