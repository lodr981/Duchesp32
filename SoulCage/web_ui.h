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
:root{--bg:#0b0b10;--card:#16161f;--line:#272734;--text:#ecebf3;--muted:#9a98ab;--accent:#8fd3ff;--warn:#ffb86b;--bad:#ff6b81;--ok:#7ee2a8;--pink:#ff8fc0}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
  padding:calc(env(safe-area-inset-top) + 12px) 16px calc(env(safe-area-inset-bottom) + 90px)}
header{display:flex;align-items:center;gap:12px;margin:4px 0 14px}
header img{width:44px;height:44px;border-radius:50%}
header h1{font-size:22px;margin:0}
header small{color:var(--muted)}
.chips{margin-left:auto;display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}
.chip{background:var(--card);border:1px solid var(--line);border-radius:999px;padding:3px 10px;font-size:13px;color:var(--muted);white-space:nowrap}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:16px;margin:0 0 14px}
.card h2{font-size:15px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted);margin:0 0 12px}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:10px 0}
.row label{flex:1}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:8px 0}
.grid3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;margin:8px 0}
button{font:inherit;border:0;border-radius:12px;padding:12px 10px;background:#24243a;color:var(--text);cursor:pointer}
button.big{display:flex;flex-direction:column;align-items:center;gap:2px;padding:12px 6px;font-size:14px}
button.big span{font-size:28px;line-height:1.1}
button.primary{background:var(--accent);color:#06121b;font-weight:600}
button.danger{background:#3a1f27;color:var(--bad)}
button.active{outline:2px solid var(--accent);background:#1d2b38}
button:disabled{opacity:.5}
button:active{transform:scale(.97)}
input,select{font:inherit;color:var(--text);background:#0f0f16;border:1px solid var(--line);border-radius:10px;padding:10px;width:100%}
input[type=range]{padding:0;accent-color:var(--accent)}
input[type=checkbox]{width:24px;height:24px;accent-color:var(--accent)}
.muted{color:var(--muted);font-size:14px}
.msg{min-height:1.4em;font-size:14px;color:var(--muted);margin-top:8px}
.msg.err{color:var(--bad)}.msg.ok{color:var(--ok)}
.bar{height:10px;background:#0f0f16;border-radius:5px;overflow:hidden;margin:6px 0 10px}
.bar div{height:100%;width:0;background:var(--accent);transition:width .4s}
.stat{display:grid;grid-template-columns:28px 1fr 40px;align-items:center;gap:8px}
.stat .bar{margin:0;height:12px;border-radius:6px}
.banner{background:#33261a;color:var(--warn);border-radius:12px;padding:10px 14px;margin:0 0 14px;font-size:14px}
code{background:#0f0f16;border-radius:6px;padding:2px 6px;font-size:13px;word-break:break-all}
.pet-name{font-size:24px;font-weight:700;margin:0}
.pet-head{display:flex;align-items:center;gap:10px}
.pet-head .lvl{margin-left:auto;text-align:right}
.toast{position:fixed;left:16px;right:16px;bottom:calc(env(safe-area-inset-bottom) + 76px);background:#2a2a40;border-radius:12px;padding:12px 16px;
  text-align:center;opacity:0;transition:opacity .3s;pointer-events:none}
.toast.show{opacity:1}
nav{position:fixed;left:0;right:0;bottom:0;display:flex;background:#12121a;border-top:1px solid var(--line);padding:6px 6px calc(env(safe-area-inset-bottom) + 6px)}
nav button{flex:1;background:none;font-size:12px;color:var(--muted);display:flex;flex-direction:column;align-items:center;padding:6px}
nav button span{font-size:22px}
nav button.on{color:var(--accent)}
.tab{display:none}.tab.on{display:block}
.seg{display:flex;gap:6px}.seg button{flex:1;padding:10px 4px;font-size:14px}
#login{position:fixed;inset:0;background:var(--bg);display:none;align-items:center;justify-content:center;padding:24px;z-index:5}
#login .card{width:100%;max-width:360px}
.hidden{display:none!important}
details summary{cursor:pointer;color:var(--muted);margin:8px 0}
.lean{height:8px;background:#0f0f16;border-radius:4px;position:relative;margin:8px 0}
.lean div{position:absolute;top:-3px;width:14px;height:14px;border-radius:7px;background:var(--accent);left:calc(50% - 7px);transition:left .3s}
</style>
</head>
<body>
<header>
  <img src="/icon.png" alt="">
  <div><h1>SoulCage</h1><small id="fw">…</small></div>
  <div class="chips"><span class="chip" id="batChip">🔋 –</span><span class="chip" id="wifiChip">📶 –</span></div>
</header>

<div class="banner hidden" id="pwBanner">Používáš výchozí heslo <b>soulcage</b>. Změň ho v Nastavení → Zabezpečení.</div>

<!-- ============ PET ============ -->
<div class="tab" id="tab-pet">
<section class="card">
  <div class="pet-head">
    <div><p class="pet-name" id="petName">…</p><span class="muted" id="petMood"></span></div>
    <div class="lvl"><b id="petLevel"></b><br><span class="muted" id="petTitle"></span></div>
  </div>
  <div class="bar" style="margin-top:12px"><div id="xpBar" style="background:var(--pink)"></div></div>
  <div class="muted" id="xpText"></div>
</section>

<section class="card">
  <h2>Jak se má</h2>
  <div class="stat"><span>🍙</span><div class="bar"><div id="foodBar"></div></div><span id="foodVal"></span></div>
  <div class="muted" style="margin:-2px 0 8px 36px">sytost</div>
  <div class="stat"><span>🎈</span><div class="bar"><div id="funBar"></div></div><span id="funVal"></span></div>
  <div class="muted" style="margin:-2px 0 8px 36px">zábava</div>
  <div class="stat"><span>⚡</span><div class="bar"><div id="energyBar"></div></div><span id="energyVal"></span></div>
  <div class="muted" style="margin:-2px 0 0 36px">energie</div>
</section>

<section class="card">
  <h2>Nakrmit</h2>
  <div class="grid2">
    <button class="big" data-food="bamboo"><span>🎋</span>Bambus</button>
    <button class="big" data-food="cookie"><span>🍪</span>Sušenka</button>
    <button class="big" data-food="pie"><span>🥧</span>Dýňový koláč</button>
    <button class="big" data-food="candy"><span>🍬</span>Bonbon</button>
  </div>
</section>

<section class="card">
  <h2>Péče</h2>
  <div class="grid3">
    <button class="big" data-pet="play"><span>🎃</span>Hrát si</button>
    <button class="big" data-pet="pet"><span>💜</span>Pohladit</button>
    <button class="big" id="sleepBtn" data-pet="sleep"><span>💤</span>Uspat</button>
  </div>
  <p class="muted">Na přívěsku: 2× poklepat = pohladit, zatřást = lekne se, hodně zatřást = zamotá se mu hlava. Když leží v klidu, usne.</p>
</section>
</div>

<!-- ============ GHOST ============ -->
<div class="tab" id="tab-ghost">
<section class="card">
  <h2>Povely</h2>
  <div class="grid3" id="actions">
    <button class="big" data-act="wave"><span>👋</span>Zamávej</button>
    <button class="big" data-act="dance"><span>💃</span>Tancuj</button>
    <button class="big" data-act="boo"><span>👻</span>BAF!</button>
    <button class="big" data-act="hide"><span>🫥</span>Schovej se</button>
    <button class="big" data-act="spin"><span>🌀</span>Zatoč se</button>
    <button class="big" data-act="laugh"><span>😂</span>Smích</button>
    <button class="big" data-act="love"><span>💜</span>Srdíčka</button>
    <button class="big" data-act="scared"><span>😱</span>Lekni se</button>
    <button class="big" data-act="dizzy"><span>😵‍💫</span>Motej se</button>
  </div>
</section>

<section class="card">
  <h2>Scéna</h2>
  <div class="grid2" id="scenes"></div>
  <div id="soulsBox" class="hidden">
    <p class="muted">Uvězněné duše:</p>
    <div class="grid2" id="souls"></div>
    <div class="row"><label for="speed">Rychlost <span class="muted" id="speedVal"></span></label></div>
    <input type="range" id="speed" min="30" max="300" step="10" dir="rtl">
    <div class="row"><label for="intro">Úvodní animace po zapnutí</label><input type="checkbox" id="intro"></div>
  </div>
</section>

<section class="card">
  <h2>Vzhled</h2>
  <p class="muted">Barva strašidelných očí</p>
  <div class="seg" id="eyeColors"></div>
  <p class="muted">Strašidelnost</p>
  <div class="seg" id="spooky">
    <button data-v="0">🧸 Roztomilý</button><button data-v="1">👻 Občas</button><button data-v="2">💀 Hodně</button>
  </div>
</section>

<section class="card">
  <h2>Pohybový senzor</h2>
  <div class="row"><span>Stav</span><b id="imuText">–</b></div>
  <div class="lean"><div id="leanDot"></div></div>
  <p class="muted">Pověs přívěsek rovně a stiskni Kalibrovat. Když duch při naklonění uhýbá na špatnou stranu, zapni otočení.</p>
  <div class="grid2"><button id="calibrate">Kalibrovat</button><button id="tiltInvertBtn">Otočit směr</button></div>
  <div class="row"><label for="motionSensitivity">Citlivost</label>
    <select id="motionSensitivity" style="width:auto"><option value="0">nízká</option><option value="1">střední</option><option value="2">vysoká</option></select></div>
  <div class="msg" id="imuMsg"></div>
</section>
</div>

<!-- ============ SETTINGS ============ -->
<div class="tab" id="tab-settings">
<section class="card">
  <h2>Displej</h2>
  <div class="row"><label for="displayOn">Displej zapnutý</label><input type="checkbox" id="displayOn"></div>
  <div class="row"><label for="brightness">Jas <span class="muted" id="brightnessVal"></span></label></div>
  <input type="range" id="brightness" min="5" max="100" step="5">
</section>

<section class="card">
  <h2>Baterie a úspora</h2>
  <div class="row"><span>Baterie</span><b id="batText">–</b></div>
  <div class="row"><label for="sleepMinutes">Vypnout po</label>
    <select id="sleepMinutes" style="width:auto">
      <option value="0">nikdy</option><option value="5">5 min</option><option value="15">15 min</option>
      <option value="30">30 min</option><option value="60">1 hodině</option><option value="120">2 hodinách</option>
      <option value="240">4 hodinách</option>
    </select></div>
  <p class="muted">Probuzení: vypnout a zapnout vypínačem (nebo tlačítko BOOT).</p>
  <button class="danger" id="sleepNow">Vypnout teď (hluboký spánek)</button>
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
  <code id="exBoo"></code> – BAF!<br>
  <code id="exFeed"></code> – nakrmit<br>
  <code id="exScene"></code> – scéna (0–4, 5 = duše)<br>
  <code id="exBright"></code> – jas</p>
</section>

<section class="card">
  <h2>Mazlíček</h2>
  <div class="row"><input id="petNameInput" maxlength="20" placeholder="Jméno ducha"><button id="savePetName" style="white-space:nowrap">Přejmenovat</button></div>
  <button class="danger" id="resetPet">Nový mazlíček (začít od levelu 1)</button>
</section>

<section class="card">
  <h2>Zabezpečení</h2>
  <div class="row"><input type="password" id="newPassword" placeholder="Nové heslo (min. 8 znaků)" autocomplete="new-password"></div>
  <button id="savePassword">Změnit heslo</button>
  <div class="msg" id="pwMsg"></div>
</section>

<button id="reboot" style="width:100%">Restartovat přívěsek</button>
</div>

<div class="toast" id="toast"></div>

<nav>
  <button data-tab="pet"><span>🐼</span>Mazlíček</button>
  <button data-tab="ghost"><span>👻</span>Duch</button>
  <button data-tab="settings"><span>⚙️</span>Nastavení</button>
</nav>

<div id="login"><div class="card">
  <h2>Přihlášení</h2>
  <p class="muted">Heslo zařízení (výchozí <b>soulcage</b>).</p>
  <input type="password" id="loginPassword" autocomplete="current-password">
  <div style="margin-top:12px"><button class="primary" id="loginBtn" style="width:100%">Přihlásit</button></div>
  <div class="msg err" id="loginMsg"></div>
</div></div>

<script>
const $ = id => document.getElementById(id);
const store = { get(k) { try { return localStorage.getItem(k) || ''; } catch (e) { return ''; } },
                set(k, v) { try { localStorage.setItem(k, v); } catch (e) {} } };
let auth = store.get('sc_auth');
let state = null, formsFilled = false, offline = 0, toastTimer;

function b64(s) { return btoa(String.fromCharCode(...new TextEncoder().encode(s))); }
function setAuth(pw) { auth = 'Basic ' + b64('admin:' + pw); store.set('sc_auth', auth); }

async function api(path, params, method) {
  const opts = { method: method || (params ? 'POST' : 'GET'), headers: { Authorization: auth } };
  if (params) opts.body = new URLSearchParams(params);
  const r = await fetch(path, opts);
  if (r.status === 401) { showLogin(); throw new Error('Nepřihlášeno'); }
  const j = await r.json().catch(() => ({}));
  if (!r.ok || j.ok === false) throw new Error(j.error || j.message || ('HTTP ' + r.status));
  return j;
}

function msg(id, text, cls) { const el = $(id); el.textContent = text || ''; el.className = 'msg ' + (cls || ''); }
function toast(text) { const t = $('toast'); t.textContent = text; t.classList.add('show'); clearTimeout(toastTimer); toastTimer = setTimeout(() => t.classList.remove('show'), 2200); }
function showLogin() { $('login').style.display = 'flex'; }

function showTab(name) {
  document.querySelectorAll('.tab').forEach(t => t.classList.toggle('on', t.id === 'tab-' + name));
  document.querySelectorAll('nav button').forEach(b => b.classList.toggle('on', b.dataset.tab === name));
  store.set('sc_tab', name);
  window.scrollTo(0, 0);
}
document.querySelectorAll('nav button').forEach(b => b.onclick = () => showTab(b.dataset.tab));
showTab(store.get('sc_tab') || 'pet');

$('loginBtn').onclick = async () => {
  setAuth($('loginPassword').value);
  try { await refresh(); $('login').style.display = 'none'; msg('loginMsg', ''); }
  catch (e) { msg('loginMsg', 'Špatné heslo', 'err'); }
};

function statBar(id, v) {
  $(id + 'Bar').style.width = v + '%';
  $(id + 'Bar').style.background = v < 25 ? 'var(--bad)' : v < 50 ? 'var(--warn)' : 'var(--ok)';
  $(id + 'Val').textContent = v + '%';
}

function buttons(container, items, onClick) {
  const el = $(container);
  if (el.children.length !== items.length) {
    el.innerHTML = '';
    items.forEach((name, i) => {
      const b = document.createElement('button');
      b.textContent = name;
      b.onclick = () => onClick(i);
      el.appendChild(b);
    });
  }
  return [...el.children];
}

function render(s) {
  state = s;
  $('fw').textContent = 'verze ' + s.fw;
  $('fwText').textContent = s.fw;
  $('pwBanner').classList.toggle('hidden', !s.defaultPassword);
  const b = s.battery;
  $('batText').textContent = b.present ? b.percent + ' % (' + (b.mv / 1000).toFixed(2) + ' V)' : 'napájení z USB';
  $('batChip').textContent = '🔋 ' + (b.present ? b.percent + ' %' : 'USB');
  const off = s.wifiOffIn >= 0 ? ', vypnu za ' + Math.ceil(s.wifiOffIn / 60) + ' min' : '';
  $('netText').textContent = (s.mode === 'ap' ? 'hotspot ' : '') + s.ssid + ' · ' + s.ip + off;
  $('wifiChip').textContent = '📶 ' + (s.mode === 'ap' ? 'hotspot' : s.rssi + ' dBm');

  // Pet
  const p = s.pet;
  $('petName').textContent = p.name;
  $('petMood').textContent = p.mood;
  $('petLevel').textContent = 'Level ' + p.level;
  $('petTitle').textContent = p.title;
  const span = p.xpNext ? p.xpNext - p.xpLevel : 1;
  $('xpBar').style.width = (p.xpNext ? Math.min(100, (p.xp - p.xpLevel) * 100 / span) : 100) + '%';
  $('xpText').textContent = p.xpNext ? 'Do dalšího levelu: ' + (p.xpNext - p.xp) + ' bodů péče' : 'Nejvyšší level – král duchů! 👑';
  statBar('food', p.food); statBar('fun', p.fun); statBar('energy', p.energy);
  const sb = $('sleepBtn');
  sb.dataset.pet = p.asleep ? 'wake' : 'sleep';
  sb.innerHTML = p.asleep ? '<span>☀️</span>Probudit' : '<span>💤</span>Uspat';

  // Ghost
  const ghostMode = s.displayMode === 0;
  buttons('scenes', [...s.scenes, 'Uvězněné duše'], i => api('/api/scene', { id: i }).then(refresh))
    .forEach((btn, i) => btn.classList.toggle('active', ghostMode ? i === s.scene : i === s.scenes.length));
  $('soulsBox').classList.toggle('hidden', ghostMode);
  buttons('souls', s.souls, i => api('/api/soul', { id: i }).then(refresh))
    .forEach((btn, i) => btn.classList.toggle('active', !ghostMode && i === s.soul));
  buttons('eyeColors', s.eyeColors, i => saveSettings({ eyeColor: i }).then(refresh))
    .forEach((btn, i) => btn.classList.toggle('active', i === s.eyeColor));
  document.querySelectorAll('#spooky button').forEach(btn => btn.classList.toggle('active', +btn.dataset.v === s.spooky));
  $('imuText').textContent = s.imu.ok ? (s.imu.calibrated ? 'funguje' : 'funguje, nekalibrováno') : 'nenalezen';
  $('leanDot').style.left = 'calc(' + (50 + s.imu.lean * 45) + '% - 7px)';
  $('tiltInvertBtn').classList.toggle('active', s.tiltInvert);

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
    $('motionSensitivity').value = s.motionSensitivity;
    $('petNameInput').value = p.name;
    $('networks').innerHTML = s.networks.map((n, i) =>
      '<div class="grid2"><input id="ssid' + i + '" placeholder="Síť ' + (i + 1) + '" autocapitalize="off"><input id="pass' + i +
      '" type="password" placeholder="' + (n ? 'heslo beze změny' : 'heslo') + '"></div>').join('');
    s.networks.forEach((n, i) => $('ssid' + i).value = n);
    const base = 'http://' + (s.mode === 'ap' ? s.ip : s.hostname + '.local');
    $('exBoo').textContent = base + '/api/ghost?action=boo';
    $('exFeed').textContent = base + '/api/pet?cmd=feed&food=bamboo';
    $('exScene').textContent = base + '/api/scene?id=0';
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

function saveSettings(params) { return api('/api/settings', params).catch(e => toast(e.message)); }
function petCmd(params) { return api('/api/pet', params).then(j => { toast(j.message); refresh(); }).catch(e => toast(e.message)); }

document.querySelectorAll('[data-food]').forEach(b => b.onclick = () => petCmd({ cmd: 'feed', food: b.dataset.food }));
document.querySelectorAll('[data-pet]').forEach(b => b.onclick = () => petCmd({ cmd: b.dataset.pet }));
document.querySelectorAll('[data-act]').forEach(b => b.onclick = () => api('/api/ghost', { action: b.dataset.act }).catch(e => toast(e.message)));
document.querySelectorAll('#spooky button').forEach(b => b.onclick = () => saveSettings({ spooky: b.dataset.v }).then(refresh));

$('calibrate').onclick = () => api('/api/imu/calibrate', {}).then(() => { msg('imuMsg', 'Hotovo, poloha uložena', 'ok'); refresh(); })
  .catch(e => msg('imuMsg', e.message, 'err'));
$('tiltInvertBtn').onclick = () => saveSettings({ tiltInvert: state.tiltInvert ? 0 : 1 }).then(refresh);
$('motionSensitivity').onchange = e => saveSettings({ motionSensitivity: e.target.value });

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

$('savePetName').onclick = () => petCmd({ cmd: 'name', name: $('petNameInput').value });
$('resetPet').onclick = () => { if (confirm('Opravdu začít s novým mazlíčkem? Level a body péče se smažou.')) petCmd({ cmd: 'reset' }); };

$('sleepNow').onclick = () => {
  if (confirm('Vypnout přívěsek? Probudíš ho vypínačem.')) api('/api/sleep', {}).then(() => msg('wifiMsg', 'Přívěsek spí.'));
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
