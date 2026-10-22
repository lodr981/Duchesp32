#pragma once

// Single-page control UI served at http://soulcage.local/ (or 192.168.4.1 in hotspot mode).
static const char WEB_UI[] PROGMEM = R"HTML(<!doctype html>
<html lang="cs">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="SoulCage">
<meta name="theme-color" content="#0b0b10">
<link rel="apple-touch-icon" href="/icon.png">
<link rel="manifest" href="/manifest.webmanifest">
<title>SoulCage</title>
<style>
:root{--bg:#0b0b10;--card:#16161f;--line:#272734;--text:#ecebf3;--muted:#9a98ab;--accent:#8fd3ff;--warn:#ffb86b;--bad:#ff6b81;--ok:#7ee2a8}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
  padding:calc(env(safe-area-inset-top) + 12px) 16px calc(env(safe-area-inset-bottom) + 24px)}
header{display:flex;align-items:center;gap:12px;margin:4px 0 16px}
header img{width:44px;height:44px;border-radius:50%}
header h1{font-size:22px;margin:0}
header small{color:var(--muted)}
.chips{margin-left:auto;display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}
.chip{background:var(--card);border:1px solid var(--line);border-radius:999px;padding:3px 10px;font-size:13px;color:var(--muted);white-space:nowrap}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:16px;margin:0 0 14px}
.card h2{font-size:15px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted);margin:0 0 12px}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:10px 0}
.row label{flex:1}
.souls{display:grid;grid-template-columns:1fr 1fr;gap:10px}
button{font:inherit;border:0;border-radius:12px;padding:12px 14px;background:#24243a;color:var(--text);cursor:pointer}
button.primary{background:var(--accent);color:#06121b;font-weight:600}
button.danger{background:#3a1f27;color:var(--bad)}
button.active{outline:2px solid var(--accent);background:#1d2b38}
button:disabled{opacity:.5}
input,select{font:inherit;color:var(--text);background:#0f0f16;border:1px solid var(--line);border-radius:10px;padding:10px;width:100%}
input[type=range]{padding:0;accent-color:var(--accent)}
input[type=checkbox]{width:24px;height:24px;accent-color:var(--accent)}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:8px 0}
.muted{color:var(--muted);font-size:14px}
.msg{min-height:1.4em;font-size:14px;color:var(--muted);margin-top:8px}
.msg.err{color:var(--bad)}.msg.ok{color:var(--ok)}
.bar{height:10px;background:#0f0f16;border-radius:5px;overflow:hidden;margin:10px 0}
.bar div{height:100%;width:0;background:var(--accent);transition:width .3s}
.banner{background:#33261a;color:var(--warn);border-radius:12px;padding:10px 14px;margin:0 0 14px;font-size:14px}
code{background:#0f0f16;border-radius:6px;padding:2px 6px;font-size:13px;word-break:break-all}
#login{position:fixed;inset:0;background:var(--bg);display:none;align-items:center;justify-content:center;padding:24px}
#login .card{width:100%;max-width:360px}
.hidden{display:none!important}
details summary{cursor:pointer;color:var(--muted);margin:8px 0}
</style>
</head>
<body>
<header>
  <img src="/icon.png" alt="">
  <div><h1>SoulCage</h1><small id="fw">…</small></div>
  <div class="chips"><span class="chip" id="batChip">🔋 –</span><span class="chip" id="wifiChip">📶 –</span></div>
</header>

<div class="banner hidden" id="pwBanner">Používáš výchozí heslo <b>soulcage</b>. Změň ho dole v části Zabezpečení.</div>

<section class="card">
  <h2>Duše</h2>
  <div class="souls" id="souls"></div>
  <div class="grid2"><button id="prevSoul">◀ Předchozí</button><button id="nextSoul">Další ▶</button></div>
</section>

<section class="card">
  <h2>Displej</h2>
  <div class="row"><label for="displayOn">Displej zapnutý</label><input type="checkbox" id="displayOn"></div>
  <div class="row"><label for="brightness">Jas <span class="muted" id="brightnessVal"></span></label></div>
  <input type="range" id="brightness" min="5" max="100" step="5">
  <div class="row"><label for="speed">Rychlost animace <span class="muted" id="speedVal"></span></label></div>
  <input type="range" id="speed" min="30" max="300" step="10" dir="rtl">
  <div class="row"><label for="intro">Úvodní animace po zapnutí</label><input type="checkbox" id="intro"></div>
</section>

<section class="card">
  <h2>Baterie a úspora</h2>
  <div class="row"><span>Baterie</span><b id="batText">–</b></div>
  <div class="row"><label for="sleepMinutes">Uspat po</label>
    <select id="sleepMinutes" style="width:auto">
      <option value="0">nikdy</option><option value="5">5 min</option><option value="15">15 min</option>
      <option value="30">30 min</option><option value="60">1 hodině</option><option value="120">2 hodinách</option>
      <option value="240">4 hodinách</option>
    </select></div>
  <p class="muted">Probuzení: vypnout a zapnout vypínačem (nebo tlačítko BOOT).</p>
  <button class="danger" id="sleepNow">Uspat teď</button>
</section>

<section class="card">
  <h2>Wi-Fi</h2>
  <div class="row"><span>Stav</span><b id="netText">–</b></div>
  <div class="row"><label for="wifiPolicy">Wi-Fi</label>
    <select id="wifiPolicy" style="width:auto">
      <option value="0">po zapnutí na čas</option><option value="1">stále zapnutá</option><option value="2">jen záchranný režim</option>
    </select></div>
  <div class="row" id="wifiMinutesRow"><label for="wifiMinutes">Vypnout po nečinnosti (min)</label>
    <input type="number" id="wifiMinutes" min="1" max="120" style="width:90px"></div>
  <p class="muted">Záchranný režim: přívěsek 3× rychle za sebou vypni a zapni → vytvoří hotspot <b>SoulCage-xxxx</b> (heslo = heslo zařízení), adresa <b>http://192.168.4.1</b>.</p>
  <details><summary>Uložené sítě (domácí Wi-Fi, hotspot iPhonu…)</summary>
    <div id="networks"></div>
    <div class="row"><label for="hostname">Název v síti (.local)</label><input id="hostname" style="width:150px"></div>
    <button class="primary" id="saveWifi">Uložit a připojit</button>
  </details>
  <div class="msg" id="wifiMsg"></div>
</section>

<section class="card">
  <h2>Aktualizace</h2>
  <div class="row"><span>Nainstalovaná verze</span><b id="fwText">–</b></div>
  <div class="row"><span>Nejnovější verze</span><b id="latestText">–</b></div>
  <div class="muted" id="notes"></div>
  <div class="bar hidden" id="otaBar"><div></div></div>
  <div class="grid2"><button id="otaCheck">Zkontrolovat</button><button class="primary" id="otaInstall" disabled>Nainstalovat</button></div>
  <details><summary>Nahrát firmware ze souboru (.bin)</summary>
    <input type="file" id="otaFile" accept=".bin">
    <div style="margin-top:8px"><button id="otaUpload">Nahrát</button></div>
  </details>
  <details><summary>Zdroj aktualizací</summary>
    <input id="manifestUrl">
    <div style="margin-top:8px"><button id="saveManifest">Uložit</button></div>
  </details>
  <div class="msg" id="otaMsg"></div>
</section>

<section class="card">
  <h2>Zkratky pro iPhone</h2>
  <p class="muted">V aplikaci Zkratky přidej akci <b>Získat obsah URL</b>, metoda <b>POST</b>, a hlavičku
  <code>Authorization</code> s hodnotou:</p>
  <p><code id="authHeader">–</code></p>
  <p class="muted">Příklady URL:<br>
  <code id="exNext"></code> – další duše<br>
  <code id="exSoul"></code> – konkrétní duše (0, 1…)<br>
  <code id="exDisplay"></code> – displej zap/vyp<br>
  <code id="exBright"></code> – jas</p>
</section>

<section class="card">
  <h2>Zabezpečení</h2>
  <div class="row"><input type="password" id="newPassword" placeholder="Nové heslo (min. 8 znaků)" autocomplete="new-password"></div>
  <button id="savePassword">Změnit heslo</button>
  <div class="msg" id="pwMsg"></div>
</section>

<button id="reboot" style="width:100%">Restartovat přívěsek</button>

<div id="login"><div class="card">
  <h2>Přihlášení</h2>
  <p class="muted">Heslo zařízení (výchozí <b>soulcage</b>).</p>
  <input type="password" id="loginPassword" autocomplete="current-password">
  <div style="margin-top:12px"><button class="primary" id="loginBtn" style="width:100%">Přihlásit</button></div>
  <div class="msg err" id="loginMsg"></div>
</div></div>

<script>
const $ = id => document.getElementById(id);
let auth = '';
try { auth = localStorage.getItem('sc_auth') || ''; } catch (e) {}
let state = null, formsFilled = false, offline = 0;

function b64(s) { return btoa(String.fromCharCode(...new TextEncoder().encode(s))); }
function setAuth(pw) { auth = 'Basic ' + b64('admin:' + pw); try { localStorage.setItem('sc_auth', auth); } catch (e) {} }

async function api(path, params, method) {
  const opts = { method: method || (params ? 'POST' : 'GET'), headers: { Authorization: auth } };
  if (params) opts.body = new URLSearchParams(params);
  const r = await fetch(path, opts);
  if (r.status === 401) { showLogin(); throw new Error('Nepřihlášeno'); }
  const j = await r.json().catch(() => ({}));
  if (!r.ok || j.ok === false) throw new Error(j.error || ('HTTP ' + r.status));
  return j;
}

function msg(id, text, cls) { const el = $(id); el.textContent = text || ''; el.className = 'msg ' + (cls || ''); }
function showLogin() { $('login').style.display = 'flex'; }

$('loginBtn').onclick = async () => {
  setAuth($('loginPassword').value);
  try { await refresh(); $('login').style.display = 'none'; msg('loginMsg', ''); }
  catch (e) { msg('loginMsg', 'Špatné heslo', 'err'); }
};

function render(s) {
  state = s;
  $('fw').textContent = 'verze ' + s.fw;
  $('fwText').textContent = s.fw;
  $('pwBanner').classList.toggle('hidden', !s.defaultPassword);
  const b = s.battery;
  const batText = b.present ? b.percent + ' % (' + (b.mv / 1000).toFixed(2) + ' V)' : 'napájení z USB';
  $('batText').textContent = batText;
  $('batChip').textContent = '🔋 ' + (b.present ? b.percent + ' %' : 'USB');
  const off = s.wifiOffIn >= 0 ? ', vypnu za ' + Math.ceil(s.wifiOffIn / 60) + ' min' : '';
  $('netText').textContent = (s.mode === 'ap' ? 'hotspot ' : '') + s.ssid + ' · ' + s.ip + off;
  $('wifiChip').textContent = '📶 ' + (s.mode === 'ap' ? 'hotspot' : s.rssi + ' dBm');

  const souls = $('souls');
  if (souls.children.length !== s.souls.length) {
    souls.innerHTML = '';
    s.souls.forEach((name, i) => {
      const btn = document.createElement('button');
      btn.textContent = name;
      btn.onclick = () => api('/api/soul', { id: i }).then(refresh).catch(e => alert(e.message));
      souls.appendChild(btn);
    });
  }
  [...souls.children].forEach((btn, i) => btn.classList.toggle('active', i === s.soul));
  $('displayOn').checked = s.display;

  if (!formsFilled) {
    formsFilled = true;
    $('brightness').value = s.brightness;
    $('speed').value = s.frameDelay;
    $('intro').checked = s.intro;
    $('sleepMinutes').value = s.sleepMinutes;
    $('wifiPolicy').value = s.wifiPolicy;
    $('wifiMinutes').value = s.wifiMinutes;
    $('hostname').value = s.hostname;
    $('manifestUrl').value = s.manifestUrl;
    $('networks').innerHTML = s.networks.map((n, i) =>
      '<div class="grid2"><input id="ssid' + i + '" placeholder="Síť ' + (i + 1) + '" autocapitalize="off"><input id="pass' + i +
      '" type="password" placeholder="' + (n ? 'heslo beze změny' : 'heslo') + '"></div>').join('');
    s.networks.forEach((n, i) => $('ssid' + i).value = n);
    const base = 'http://' + (s.mode === 'ap' ? s.ip : s.hostname + '.local');
    $('exNext').textContent = base + '/api/soul?step=next';
    $('exSoul').textContent = base + '/api/soul?id=0';
    $('exDisplay').textContent = base + '/api/display';
    $('exBright').textContent = base + '/api/settings?brightness=50';
    $('authHeader').textContent = auth;
  }
  $('brightnessVal').textContent = $('brightness').value + ' %';
  $('speedVal').textContent = Math.round(1000 / $('speed').value) + ' snímků/s';
  $('wifiMinutesRow').classList.toggle('hidden', $('wifiPolicy').value !== '0');

  const o = s.ota;
  $('latestText').textContent = o.latest || '–';
  $('notes').textContent = o.notes || '';
  const busy = ['checking', 'downloading', 'uploading', 'done'].includes(o.state);
  $('otaBar').classList.toggle('hidden', !(o.state === 'downloading' || o.state === 'uploading' || o.state === 'done'));
  $('otaBar').firstElementChild.style.width = o.progress + '%';
  $('otaInstall').disabled = busy || !o.available;
  $('otaCheck').disabled = busy;
  if (o.message) msg('otaMsg', o.message, o.state === 'error' ? 'err' : o.state === 'done' ? 'ok' : '');
}

async function refresh() {
  const s = await api('/api/status');
  offline = 0;
  render(s);
}

function saveSettings(params) { return api('/api/settings', params).catch(e => alert(e.message)); }

$('prevSoul').onclick = () => api('/api/soul', { step: 'prev' }).then(refresh);
$('nextSoul').onclick = () => api('/api/soul', { step: 'next' }).then(refresh);
$('displayOn').onchange = e => api('/api/display', { on: e.target.checked ? 1 : 0 });
$('brightness').oninput = () => $('brightnessVal').textContent = $('brightness').value + ' %';
$('brightness').onchange = () => saveSettings({ brightness: $('brightness').value });
$('speed').oninput = () => $('speedVal').textContent = Math.round(1000 / $('speed').value) + ' snímků/s';
$('speed').onchange = () => saveSettings({ frameDelay: $('speed').value });
$('intro').onchange = e => saveSettings({ intro: e.target.checked ? 1 : 0 });
$('sleepMinutes').onchange = e => saveSettings({ sleepMinutes: e.target.value });
$('wifiPolicy').onchange = e => { $('wifiMinutesRow').classList.toggle('hidden', e.target.value !== '0'); saveSettings({ wifiPolicy: e.target.value }); };
$('wifiMinutes').onchange = e => saveSettings({ wifiMinutes: e.target.value });
$('saveManifest').onclick = () => saveSettings({ manifestUrl: $('manifestUrl').value }).then(() => msg('otaMsg', 'Uloženo', 'ok'));

$('sleepNow').onclick = () => {
  if (confirm('Uspat přívěsek? Probudíš ho vypínačem.')) api('/api/sleep', {}).then(() => msg('wifiMsg', 'Přívěsek spí.'));
};
$('reboot').onclick = () => { if (confirm('Restartovat přívěsek?')) api('/api/reboot', {}); };

$('saveWifi').onclick = async () => {
  const p = { hostname: $('hostname').value };
  state.networks.forEach((n, i) => { p['ssid' + i] = $('ssid' + i).value; p['pass' + i] = $('pass' + i).value; });
  try {
    await saveSettings({ hostname: p.hostname });
    await api('/api/wifi', p);
    msg('wifiMsg', 'Uloženo. Přívěsek se připojuje – za chvíli ho najdeš na http://' + p.hostname + '.local (iPhone musí být ve stejné síti).', 'ok');
  } catch (e) { msg('wifiMsg', e.message, 'err'); }
};

$('savePassword').onclick = async () => {
  const pw = $('newPassword').value;
  try { await api('/api/password', { password: pw }); setAuth(pw); formsFilled = false; $('newPassword').value = ''; msg('pwMsg', 'Heslo změněno', 'ok'); }
  catch (e) { msg('pwMsg', e.message, 'err'); }
};

$('otaCheck').onclick = () => api('/api/ota/check', {}).then(refresh).catch(e => msg('otaMsg', e.message, 'err'));
$('otaInstall').onclick = () => {
  if (confirm('Nainstalovat verzi ' + state.ota.latest + '? Přívěsek se pak restartuje.'))
    api('/api/ota/install', {}).then(refresh).catch(e => msg('otaMsg', e.message, 'err'));
};
$('otaUpload').onclick = () => {
  const f = $('otaFile').files[0];
  if (!f) return msg('otaMsg', 'Vyber soubor .bin', 'err');
  const fd = new FormData(); fd.append('firmware', f, f.name);
  const x = new XMLHttpRequest();
  x.open('POST', '/api/ota/upload');
  x.setRequestHeader('Authorization', auth);
  $('otaBar').classList.remove('hidden');
  x.upload.onprogress = e => { if (e.lengthComputable) $('otaBar').firstElementChild.style.width = (e.loaded * 100 / e.total) + '%'; };
  x.onload = () => {
    let j = {}; try { j = JSON.parse(x.responseText); } catch (e) {}
    if (x.status === 200) msg('otaMsg', 'Nahráno, přívěsek se restartuje…', 'ok');
    else msg('otaMsg', j.error || ('Chyba ' + x.status), 'err');
  };
  x.onerror = () => msg('otaMsg', 'Spojení přerušeno', 'err');
  msg('otaMsg', 'Nahrávám…');
  x.send(fd);
};

async function loop() {
  try { await refresh(); }
  catch (e) { if (++offline > 2) $('wifiChip').textContent = '📶 offline'; }
  setTimeout(loop, 2000);
}
if (!auth) showLogin();
loop();
</script>
</body>
</html>
)HTML";
