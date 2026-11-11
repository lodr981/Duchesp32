#pragma once

// Single-page control UI served at http://<name>.local/ or http://192.168.4.1 (hotspot).
static const char WEB_UI[] PROGMEM = R"HTML(<!doctype html>
<html lang="cs">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="Můj duch">
<meta name="theme-color" content="#140b24">
<link rel="apple-touch-icon" href="/icon.png">
<link rel="manifest" href="/manifest.webmanifest">
<title>Můj duch</title>
<style>
:root{--bg:#140b24;--card:rgba(255,255,255,.06);--line:rgba(255,255,255,.1);--text:#f6f0ff;--muted:#b9a9d6;
  --pink:#ff7ac8;--lilac:#b79bff;--mint:#7ee8c4;--warn:#ffc46b;--bad:#ff6b8a}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;color:var(--text);font:16px/1.45 ui-rounded,"SF Pro Rounded",-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
  background:radial-gradient(120% 60% at 50% -10%,#3a1d63 0%,#140b24 60%) fixed,var(--bg);
  padding:calc(env(safe-area-inset-top) + 14px) 16px calc(env(safe-area-inset-bottom) + 96px)}
header{display:flex;align-items:center;gap:12px;margin:2px 0 16px}
header img{width:52px;height:52px;border-radius:50%;box-shadow:0 0 0 2px var(--pink),0 0 24px rgba(255,122,200,.45)}
header h1{font-size:23px;margin:0;line-height:1.15}
header small{color:var(--muted)}
.chips{margin-left:auto;display:flex;flex-direction:column;gap:4px;align-items:flex-end}
.chip{background:var(--card);border:1px solid var(--line);border-radius:999px;padding:2px 10px;font-size:12px;color:var(--muted);white-space:nowrap}
.card{background:var(--card);border:1px solid var(--line);border-radius:22px;padding:16px;margin:0 0 14px;backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px)}
.card h2{font-size:14px;text-transform:uppercase;letter-spacing:.08em;color:var(--lilac);margin:0;display:flex;align-items:center;gap:8px}
.card .head{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
details.help{margin:-4px 0 10px}
details.help summary{list-style:none;cursor:pointer;color:var(--muted);font-size:13px;display:inline-flex;align-items:center;gap:6px}
details.help summary::-webkit-details-marker{display:none}
details.help summary::before{content:"?";display:inline-grid;place-items:center;width:20px;height:20px;border-radius:50%;background:rgba(183,155,255,.2);color:var(--lilac);font-weight:700;font-size:12px}
details.help p,details.help ul{font-size:14px;color:var(--muted);margin:8px 0 0;padding-left:18px}
details.help p{padding-left:0}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:10px 0}
.row label{flex:1}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:8px 0}
.grid3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin:8px 0}
button{font:inherit;border:0;border-radius:16px;padding:12px 10px;background:rgba(255,255,255,.08);color:var(--text);cursor:pointer;transition:transform .1s}
button.big{display:flex;flex-direction:column;align-items:center;gap:2px;padding:12px 6px;font-size:14px}
button.big span{font-size:30px;line-height:1.1}
button.primary{background:linear-gradient(135deg,var(--pink),var(--lilac));color:#1b0c2e;font-weight:700}
button.danger{background:rgba(255,107,138,.12);color:var(--bad)}
button.active{box-shadow:0 0 0 2px var(--pink) inset;background:rgba(255,122,200,.16)}
button:disabled{opacity:.45}
button:active{transform:scale(.95)}
input,select{font:inherit;color:var(--text);background:rgba(0,0,0,.25);border:1px solid var(--line);border-radius:12px;padding:10px;width:100%}
input[type=range]{padding:0;accent-color:var(--pink)}
input[type=checkbox]{width:24px;height:24px;accent-color:var(--pink)}
.muted{color:var(--muted);font-size:14px}
.msg{min-height:1.4em;font-size:14px;color:var(--muted);margin-top:8px}
.msg.err{color:var(--bad)}.msg.ok{color:var(--mint)}
.bar{height:10px;background:rgba(0,0,0,.3);border-radius:5px;overflow:hidden;margin:6px 0 10px}
.bar div{height:100%;width:0;background:var(--pink);transition:width .4s}
.stat{display:grid;grid-template-columns:30px 1fr 44px;align-items:center;gap:8px;margin:8px 0}
.stat span:first-child{font-size:22px}
.stat .bar{margin:0;height:14px;border-radius:7px}
.banner{background:rgba(255,196,107,.12);color:var(--warn);border-radius:16px;padding:10px 14px;margin:0 0 14px;font-size:14px}
code{background:rgba(0,0,0,.3);border-radius:6px;padding:2px 6px;font-size:13px;word-break:break-all}
.pet-name{font-size:26px;font-weight:800;margin:0}
.pet-head{display:flex;align-items:center;gap:10px}
.pet-head .lvl{margin-left:auto;text-align:right}
.lvl b{background:linear-gradient(135deg,var(--pink),var(--lilac));-webkit-background-clip:text;background-clip:text;color:transparent;font-size:18px}
.mood{display:inline-block;margin-top:4px;padding:2px 10px;border-radius:999px;background:rgba(126,232,196,.14);color:var(--mint);font-size:13px}
.surprise{display:flex;align-items:center;gap:12px;padding:14px;border-radius:18px;background:linear-gradient(135deg,rgba(255,122,200,.18),rgba(183,155,255,.18));border:1px solid var(--line);width:100%;text-align:left}
.surprise span{font-size:34px}
.surprise.active{box-shadow:0 0 0 2px var(--pink) inset}
.gest{display:grid;grid-template-columns:34px 1fr;gap:6px 10px;font-size:14px;color:var(--muted);align-items:center}
.gest b{color:var(--text);font-weight:600}
.toast{position:fixed;left:16px;right:16px;bottom:calc(env(safe-area-inset-bottom) + 84px);background:rgba(40,22,70,.95);border:1px solid var(--line);border-radius:16px;padding:12px 16px;
  text-align:center;opacity:0;transition:opacity .3s;pointer-events:none;z-index:4}
.toast.show{opacity:1}
nav{position:fixed;left:12px;right:12px;bottom:calc(env(safe-area-inset-bottom) + 8px);display:flex;background:rgba(30,16,52,.9);border:1px solid var(--line);
  border-radius:22px;padding:6px;backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px);z-index:3}
nav button{flex:1;background:none;font-size:12px;color:var(--muted);display:flex;flex-direction:column;align-items:center;padding:6px;border-radius:16px}
nav button span{font-size:22px}
nav button.on{color:var(--text);background:rgba(255,122,200,.16)}
.tab{display:none}.tab.on{display:block}
.seg{display:flex;gap:6px;flex-wrap:wrap}.seg button{flex:1;padding:10px 6px;font-size:14px}
.parents{border-style:dashed}
#login{position:fixed;inset:0;background:var(--bg);display:none;align-items:center;justify-content:center;padding:24px;z-index:5}
#login .card{width:100%;max-width:360px}
.hidden{display:none!important}
.souls-only .ghost-only{display:none!important}
.souls-card{display:none}.souls-only .souls-card{display:block}
textarea{font:inherit;color:var(--text);background:rgba(0,0,0,.25);border:1px solid var(--line);border-radius:12px;padding:10px;width:100%;min-height:54px;margin:4px 0 8px}
details.more summary{cursor:pointer;color:var(--muted);margin:8px 0}
.lean{height:8px;background:rgba(0,0,0,.3);border-radius:4px;position:relative;margin:8px 0}
.lean div{position:absolute;top:-3px;width:14px;height:14px;border-radius:7px;background:var(--pink);left:calc(50% - 7px);transition:left .3s}
</style>
</head>
<body>
<header>
  <img src="/icon.png" alt="">
  <div><h1 id="hello">Ahoj! 👋</h1><small id="fw">…</small></div>
  <div class="chips"><span class="chip" id="batChip">🔋 –</span><span class="chip" id="wifiChip">📶 –</span></div>
</header>

<div class="banner hidden" id="pwBanner">🔐 Heslo je pořád výchozí <b>soulcage</b>. Ať ho rodiče změní v Nastavení → Heslo.</div>

<!-- ============ GHOST / PET ============ -->
<div class="tab" id="tab-pet">
<section class="card souls-card">
  <div class="head"><h2>👻 Uvězněná duše</h2></div>
  <div class="grid2" id="soulsTop"></div>
  <div class="grid3">
    <button class="big" data-soul="bang"><span>✋</span>Buš</button>
    <button class="big" data-soul="knock"><span>🤜</span>Klepej</button>
    <button class="big" data-soul="escape"><span>💥</span>Útěk</button>
  </div>
  <div class="row"><label for="speed2">Rychlost <span class="muted" id="speedVal2"></span></label></div>
  <input type="range" id="speed2" min="30" max="300" step="10" dir="rtl">
  <details class="help"><summary>Co duše umí?</summary>
    <div class="gest" style="margin-top:8px">
      <span>👆</span><span><b>1× ťukni</b> na přívěsek – zaklepe zpátky</span>
      <span>✌️</span><span><b>2× ťukni</b> – buší do skla a sklo praskne</span>
      <span>🫨</span><span><b>Zatřes</b> – vyleká se a buší</span>
      <span>💥</span><span><b>Hodně zatřes</b> – rozbije sklo a uteče</span>
      <span>🙃</span><span><b>Vzhůru nohama</b> – visí hlavou dolů</span>
      <span>😴</span><span><b>Nech ji v klidu</b> – vybledne a usne</span>
    </div>
    <p>Muž a žena se sami střídají a pořád dělají něco jiného – buší, klepou, houpají se, zhasne světlo…</p>
  </details>
</section>

<section class="card">
  <div class="head"><h2>🏮 Lovec světýlek</h2><span class="muted" id="lightsBest"></span></div>
  <button class="primary" id="lightsPlay" style="width:100%;font-size:17px">▶ Hrát</button>
  <div style="margin-top:12px" class="muted">🫙 Sklenice pro duši: <b id="jarText"></b></div>
  <div class="bar"><div id="jarBar" style="background:linear-gradient(90deg,#6ec3ff,var(--lilac))"></div></div>
  <details class="help"><summary>Jak se hraje?</summary>
    <ul>
      <li>Nakláněj přívěsek a vozíš lucerničku 🏮 po skle. Sbírej modrá světýlka (1 bod) a vzácná zlatá (5 bodů, rychle mizí).</li>
      <li>Když chytáš rychle za sebou, roste <b>combo ×2 … ×5</b>.</li>
      <li>Červení duchové tě honí – dotek = přijdeš o srdíčko (máš 3). <b>Ťukni</b> na přívěsek a lucernička zableskne a vyžene je (pak se 4 s dobíjí).</li>
      <li>Každých 20 s přijde nová vlna a je to těžší. Občas přiletí srdíčko navíc.</li>
      <li>Hrát jde i bez telefonu: <b>polož přívěsek displejem nahoru a 2× ťukni</b>. Ukončíš tlačítkem BOOT.</li>
      <li>Všechna chycená světýlka jdou do sklenice. Každých 100 = duše ti poděkuje 💜</li>
    </ul>
  </details>
</section>

<section class="card ghost-only">
  <div class="pet-head">
    <div><p class="pet-name" id="petName">…</p><span class="mood" id="petMood"></span></div>
    <div class="lvl"><b id="petLevel"></b><br><span class="muted" id="petTitle"></span></div>
  </div>
  <div class="bar" style="margin-top:12px"><div id="xpBar" style="background:linear-gradient(90deg,var(--pink),var(--lilac))"></div></div>
  <div class="muted" id="xpText"></div>
  <details class="help"><summary>Jak duch roste?</summary>
    <p>Za každou péči (jídlo, hraní, hlazení) dostane body. Když jich má dost, postoupí o level a dostane
    nový doplněk: mašli 🎀, tesáčky 🦷, čarodějnický klobouk 🧙 a nakonec korunu 👑. Neboj, nikdy neumře – jen je smutný, když se o něj nestaráš.</p>
  </details>
</section>

<section class="card ghost-only">
  <div class="head"><h2>Jak se má</h2></div>
  <div class="stat"><span>🍙</span><div class="bar"><div id="foodBar"></div></div><span id="foodVal"></span></div>
  <div class="stat"><span>🎈</span><div class="bar"><div id="funBar"></div></div><span id="funVal"></span></div>
  <div class="stat"><span>⚡</span><div class="bar"><div id="energyBar"></div></div><span id="energyVal"></span></div>
  <details class="help"><summary>Co znamenají ty pruhy?</summary>
    <ul><li>🍙 <b>Sytost</b> – když je nízko, má hlad a nad hlavou mu vyskočí bublina s jídlem.</li>
    <li>🎈 <b>Zábava</b> – když se nudí, je smutný. Pomůže hraní, hlazení nebo bonbon.</li>
    <li>⚡ <b>Energie</b> – když je unavený, zívá. Nech ho spát (nebo přívěsek polož) a dobije se.</li></ul>
  </details>
</section>

<section class="card ghost-only">
  <div class="head"><h2>Nakrmit</h2></div>
  <div class="grid2">
    <button class="big" data-food="bamboo"><span>🎋</span>Bambus</button>
    <button class="big" data-food="cookie"><span>🍪</span>Sušenka</button>
    <button class="big" data-food="pie"><span>🥧</span>Dýňový koláč</button>
    <button class="big" data-food="candy"><span>🍬</span>Bonbon</button>
  </div>
  <details class="help"><summary>Co mu nejvíc chutná?</summary>
    <p>🎋 bambus pořádně zasytí, 🥧 koláč zasytí nejvíc a přidá energii, 🍪 sušenka trochu zasytí a pobaví,
    🍬 bonbon hlavně pobaví. Když je plný, odmítne – zavrtí hlavou 😄</p>
  </details>
</section>

<section class="card ghost-only">
  <div class="head"><h2>Péče</h2></div>
  <div class="grid3">
    <button class="big" data-pet="play"><span>🎃</span>Hrát si</button>
    <button class="big" data-pet="pet"><span>💜</span>Pohladit</button>
    <button class="big" id="sleepBtn" data-pet="sleep"><span>💤</span>Uspat</button>
  </div>
</section>

<section class="card ghost-only">
  <div class="head"><h2>Povely</h2></div>
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
  <details class="help"><summary>Jde to i bez telefonu?</summary>
    <div class="gest" style="margin-top:8px">
      <span>🫨</span><span><b>Zatřes</b> – lekne se</span>
      <span>🌪️</span><span><b>Hodně zatřes</b> – zamotá se mu hlava</span>
      <span>👆</span><span><b>2× ťukni</b> – pohladíš ho (srdíčka)</span>
      <span>↔️</span><span><b>Nakloň</b> – odplave tím směrem</span>
      <span>🙃</span><span><b>Otoč vzhůru nohama</b> – visí a chichotá se</span>
      <span>😴</span><span><b>Nech ho chvíli v klidu</b> – usne, pohyb ho probudí</span>
    </div>
  </details>
</section>

<section class="card">
  <div class="head"><h2>🎮 Labyrint</h2><span class="muted" id="gameProgress"></span></div>
  <button class="primary" id="gamePlay" style="width:100%;font-size:17px">▶ Hrát</button>
  <button class="danger hidden" id="gameStop" style="width:100%;margin-top:8px">⏹ Konec hry</button>
  <div class="grid3" id="levels" style="grid-template-columns:repeat(5,1fr);gap:8px;margin-top:12px"></div>
  <details class="help"><summary>Jak se hraje?</summary>
    <p>Nakláněj přívěsek a dokutálej kuličku do svítícího portálu 🌀. Je 15 levelů a každý je těžší:
    od levelu 8 jsou v labyrintu díry (spadneš = znovu od startu) a od levelu 11 je tma – vidíš jen kolem kuličky 🔦.
    Za rychlost dostaneš až 3 ⭐. Každý vyhraný level duchovi přidá zábavu.</p>
    <p>Poprvé tě přívěsek naučí ovládání („drž rovně“, „nakloň doprava“, „nakloň k sobě“). Když kulička jezdí
    obráceně, dej Nastavit ovládání. Hru ukončíš tady nebo tlačítkem BOOT.</p>
  </details>
  <button id="gameCalibrate" style="width:100%">📏 Nastavit ovládání</button>
</section>

<section class="card">
  <div class="head"><h2>🔓 Osvoboď duši</h2><span class="muted" id="huntFreed"></span></div>
  <div id="huntRun" class="hidden">
    <div class="dots" id="huntDots" style="font-size:26px;letter-spacing:3px;text-align:center"></div>
    <p class="muted" style="text-align:center;margin:4px 0 10px" id="huntText"></p>
    <div class="grid2">
      <button id="huntPrint">🖨️ Vytisknout pečetě</button>
      <button class="danger" id="huntStop">⏹ Ukončit hru</button>
    </div>
  </div>
  <div id="huntNew">
    <div class="row"><label for="huntCount">Počet pečetí</label>
      <select id="huntCount" style="width:auto"><option>3</option><option>4</option><option>5</option><option selected>6</option><option>7</option><option>8</option><option>10</option></select></div>
    <button class="primary" id="huntStart" style="width:100%">▶ Připravit novou hru</button>
  </div>
  <details class="more"><summary>✏️ Nápovědy (pro toho, kdo pečetě schovává)</summary>
    <div id="huntHints"></div>
    <button id="huntSaveHints" style="width:100%">Uložit nápovědy</button>
  </details>
  <details class="help"><summary>Jak se hraje?</summary>
    <ul>
      <li>Rodič (nebo kamarádka) dá <b>Připravit novou hru</b> a <b>Vytisknout pečetě</b> – každá pečeť je papírek s QR kódem.</li>
      <li>Pečetě rozstříhá a schová po bytě. Do Nápověd může napsat, kde je další (např. „Hledej, kde spí ponožky“).</li>
      <li>Kdo pečeť najde, naskenuje ji foťákem iPhonu (připojeného k Wi-Fi přívěsku). Na mobilu se ukáže „Pečeť zlomena!“ a nápověda k další.</li>
      <li>Na přívěsku se kolem okraje odemkne zámek, duše zabuší a sklo praskne. Po poslední pečeti duše <b>uteče na svobodu</b> 🎉</li>
    </ul>
  </details>
</section>
</div>

<!-- ============ SCENES ============ -->
<div class="tab" id="tab-scenes">
<section class="card">
  <div class="row" style="margin:0"><label for="soulsOnly"><b>Jen uvězněná duše</b><br><span class="muted">panda a mazlíček jsou schovaní</span></label><input type="checkbox" id="soulsOnly"></div>
  <details class="help"><summary>Co to dělá?</summary>
    <p>Přívěsek ukazuje jen uvězněného muže a ženu za sklem – střídají se a dělají různé věci. Když to vypneš,
    vrátí se panda-duch, mazlíček a všechny scény (hřbitov, bambusy…).</p>
  </details>
</section>

<section class="card">
  <button class="surprise" id="surpriseBtn"><span>🎲</span><div><b>Překvapení</b><br><span class="muted" style="font-size:13px">Přívěsek sám střídá scény i uvězněné duše</span></div></button>
  <details class="help" style="margin-top:10px"><summary>Jak funguje Překvapení?</summary>
    <p>Každou chvilku (1–2 minuty) se duch schová a objeví se jinde – na hřbitově, v bambusech, ve strašidelném domě…
    Občas místo něj přijde i uvězněná duše za sklem. Nikdy neukáže dvakrát po sobě to samé.
    Když vybereš konkrétní scénu, Překvapení se vypne.</p>
  </details>
</section>

<section class="card ghost-only">
  <div class="head"><h2>Scéna</h2></div>
  <div class="grid2" id="scenes"></div>
  <div id="soulsBox" class="hidden">
    <p class="muted">Kterou duši?</p>
    <div class="grid2" id="souls"></div>
    <div class="grid3">
      <button class="big" data-soul="bang"><span>✋</span>Buš</button>
      <button class="big" data-soul="knock"><span>🤜</span>Klepej</button>
      <button class="big" data-soul="escape"><span>💥</span>Útěk</button>
    </div>
    <details class="help"><summary>Co dělají uvězněné duše?</summary>
      <div class="gest" style="margin-top:8px">
        <span>🫨</span><span><b>Zatřes</b> – buší do skla a sklo praská</span>
        <span>💥</span><span><b>Hodně zatřes</b> – rozbije sklo a uteče (pak se vrátí)</span>
        <span>👆</span><span><b>2× ťukni</b> – zaklepe zpátky</span>
        <span>🙃</span><span><b>Vzhůru nohama</b> – visí hlavou dolů</span>
      </div>
      <p>Praskliny se po chvilce samy zahojí.</p>
    </details>
    <div class="row"><label for="speed">Rychlost <span class="muted" id="speedVal"></span></label></div>
    <input type="range" id="speed" min="30" max="300" step="10" dir="rtl">
  </div>
</section>

<section class="card">
  <div class="head"><h2>Vzhled</h2></div>
  <p class="muted" style="margin:0 0 6px">Barva strašidelných očí</p>
  <div class="seg" id="eyeColors"></div>
  <p class="muted" style="margin:14px 0 6px">Jak moc strašit?</p>
  <div class="seg" id="spooky">
    <button data-v="0">🧸 Roztomile</button><button data-v="1">👻 Občas</button><button data-v="2">💀 Hodně</button>
  </div>
  <details class="help"><summary>Co to mění?</summary>
    <p>Barva očí se ukáže, když duch straší (BAF!) a u duší za sklem. Strašidelnost určuje, jak často sám od sebe
    udělá BAF, jestli má tesáky a jak moc praská sklo u uvězněných duší.</p>
  </details>
</section>

<section class="card">
  <div class="head"><h2>Pohybový senzor</h2></div>
  <div class="row"><span>Stav</span><b id="imuText">–</b></div>
  <div class="lean"><div id="leanDot"></div></div>
  <div class="grid2"><button id="calibrate">📏 Kalibrovat</button><button id="tiltInvertBtn">🔁 Otočit směr</button></div>
  <div class="row"><label for="motionSensitivity">Citlivost na třesení</label>
    <select id="motionSensitivity" style="width:auto"><option value="0">malá</option><option value="1">střední</option><option value="2">velká</option></select></div>
  <details class="help"><summary>K čemu je kalibrace?</summary>
    <p>Pověs přívěsek normálně na krk (nebo ho drž rovně) a ťukni na Kalibrovat – přívěsek si zapamatuje, jak „visí rovně“.
    Tečka nahoře ukazuje náklon. Když duch při naklonění plave na opačnou stranu, dej Otočit směr.
    Když se lekne moc často, dej citlivost menší.</p>
  </details>
  <div class="msg" id="imuMsg"></div>
</section>
</div>

<!-- ============ SETTINGS ============ -->
<div class="tab" id="tab-settings">
<section class="card">
  <div class="head"><h2>Můj přívěsek</h2></div>
  <div class="row"><label for="greetingInput">Uvítání po zapnutí</label></div>
  <input id="greetingInput" maxlength="30" placeholder="Ahoj Amelko!">
  <div class="row"><label for="subtitleInput">Pod tím</label></div>
  <input id="subtitleInput" maxlength="30" placeholder="Bububu!">
  <div class="row"><label for="deviceNameInput">Jméno přívěsku (Wi-Fi)</label></div>
  <input id="deviceNameInput" maxlength="30" placeholder="Amelčin duch">
  <div class="row"><label for="petNameInput">Jméno ducha</label></div>
  <input id="petNameInput" maxlength="20" placeholder="Panďulák">
  <div class="row"><label for="qrOnBoot">QR kód po zapnutí</label><input type="checkbox" id="qrOnBoot"></div>
  <div style="margin-top:12px"><button class="primary" id="saveMine" style="width:100%">Uložit</button></div>
  <details class="help"><summary>Co se kde ukáže?</summary>
    <p>Uvítání se ukáže na displeji po každém zapnutí (duch u toho zamává). Pak přívěsek ukáže QR kód –
    namiř na něj foťák iPhonu a připojíš se k jeho Wi-Fi bez psaní hesla. Druhý QR kód otevře tuhle aplikaci.
    Zatřesením nebo dvojitým ťuknutím QR kód přeskočíš. Jméno přívěsku je název jeho Wi-Fi,
    kterou uvidíš v telefonu. Po změně jména se Wi-Fi na chvilku vypne a zapne – připoj se pak znovu.</p>
  </details>
</section>

<section class="card">
  <div class="head"><h2>Displej</h2></div>
  <div class="row"><label for="displayOn">Displej zapnutý</label><input type="checkbox" id="displayOn"></div>
  <div class="row"><label for="brightness">Jas <span class="muted" id="brightnessVal"></span></label></div>
  <input type="range" id="brightness" min="5" max="100" step="5">
  <details class="help"><summary>Jak ušetřit baterku?</summary>
    <p>Menší jas = baterka vydrží déle. Když přívěsek leží v klidu, duch usne a displej se sám ztlumí.</p>
  </details>
</section>

<section class="card">
  <div class="head"><h2>Baterie</h2></div>
  <div class="row"><span>Stav</span><b id="batText">–</b></div>
  <div class="row"><label for="sleepMinutes">Úplně vypnout po</label>
    <select id="sleepMinutes" style="width:auto">
      <option value="0">nikdy</option><option value="5">5 min</option><option value="15">15 min</option>
      <option value="30">30 min</option><option value="60">1 hodině</option><option value="120">2 hodinách</option>
      <option value="240">4 hodinách</option>
    </select></div>
  <button class="danger" id="sleepNow">Vypnout teď</button>
  <details class="help"><summary>Jak ho zase zapnu?</summary>
    <p>Vypínačem: vypnout a zapnout. Mazlíček si všechno pamatuje i když je vypnutý.</p>
  </details>
</section>

<section class="card parents">
  <div class="head"><h2>Wi-Fi</h2><span class="muted">👨‍👩‍👧 pro rodiče</span></div>
  <div class="row"><span>Hotspot přívěsku</span><b id="apText">–</b></div>
  <div class="row"><span>Internet</span><b id="netText">–</b></div>
  <div class="row"><label for="wifiPolicy">Wi-Fi zapnutá</label>
    <select id="wifiPolicy" style="width:auto">
      <option value="0">po zapnutí na čas</option><option value="1">pořád</option><option value="2">jen záchranný režim</option>
    </select></div>
  <div class="row" id="wifiMinutesRow"><label for="wifiMinutes">Vypnout po nečinnosti (min)</label>
    <input type="number" id="wifiMinutes" min="1" max="120" style="width:90px"></div>
  <details class="help"><summary>Jak se k přívěsku připojím?</summary>
    <ul>
      <li>Nejsnáz: po zapnutí přívěsek ukáže QR kód – namiř na něj foťák iPhonu a ťukni na „Připojit“.</li>
      <li>Nebo ručně: v Nastavení → Wi-Fi vyber síť <b id="apName">přívěsku</b> (heslo = heslo zařízení) a otevři <b>http://192.168.4.1</b>.</li>
      <li>iPhone napíše „Bez připojení k internetu“ – to je v pořádku, internet jede dál přes mobilní data.</li>
      <li>Wi-Fi se vypne, až se k ní nikdo nepřipojuje (podle nastavení). Zapneš ji vypnutím a zapnutím přívěsku, nebo 3× rychle za sebou vypnout/zapnout (záchranný režim).</li>
      <li>Uložená domácí Wi-Fi (nebo hotspot iPhonu) je jen kvůli aktualizacím z internetu – hotspot přívěsku běží pořád.</li>
    </ul>
  </details>
  <details class="more"><summary>Uložené sítě (domácí Wi-Fi, hotspot iPhonu…)</summary>
    <div id="networks"></div>
    <div class="row"><label for="hostname">Adresa (.local)</label><input id="hostname" style="width:150px"></div>
    <button class="primary" id="saveWifi">Uložit</button>
  </details>
  <div class="msg" id="wifiMsg"></div>
</section>

<section class="card parents">
  <div class="head"><h2>Aktualizace</h2><span class="muted">👨‍👩‍👧 pro rodiče</span></div>
  <div class="row"><span>Nainstalovaná verze</span><b id="fwText">–</b></div>
  <div class="row"><span>Nejnovější verze</span><b id="latestText">–</b></div>
  <div class="muted" id="notes"></div>
  <div class="bar hidden" id="otaBar"><div></div></div>
  <div class="grid2"><button id="otaCheck">Zkontrolovat</button><button class="primary" id="otaInstall" disabled>Nainstalovat</button></div>
  <details class="help"><summary>Jak aktualizace funguje?</summary>
    <p>Přívěsek si nový firmware stáhne z GitHubu, když je připojený k Wi-Fi s internetem (domácí síť nebo hotspot iPhonu
    uložený výše). Když nová verze nenaběhne, sám se vrátí ke staré. Mazlíček i nastavení zůstanou.</p>
  </details>
  <details class="more"><summary>Nahrát firmware ze souboru (.bin)</summary>
    <input type="file" id="otaFile" accept=".bin">
    <div style="margin-top:8px"><button id="otaUpload">Nahrát</button></div>
  </details>
  <details class="more"><summary>Zdroj aktualizací</summary>
    <input id="manifestUrl">
    <div style="margin-top:8px"><button id="saveManifest">Uložit</button></div>
  </details>
  <div class="msg" id="otaMsg"></div>
</section>

<section class="card parents">
  <div class="head"><h2>Zkratky a Siri</h2><span class="muted">👨‍👩‍👧 pro rodiče</span></div>
  <p class="muted">V aplikaci Zkratky přidej akci <b>Získat obsah URL</b>, metoda <b>POST</b>, a hlavičku
  <code>Authorization</code> s hodnotou:</p>
  <p><code id="authHeader">–</code></p>
  <p class="muted">Příklady URL:<br>
  <code id="exBoo"></code> – BAF!<br>
  <code id="exFeed"></code> – nakrmit<br>
  <code id="exScene"></code> – scéna (0–4, 5 = duše)<br>
  <code id="exBright"></code> – jas</p>
</section>

<section class="card parents">
  <div class="head"><h2>Heslo</h2><span class="muted">👨‍👩‍👧 pro rodiče</span></div>
  <div class="row"><input type="password" id="newPassword" placeholder="Nové heslo (min. 8 znaků)" autocomplete="new-password"></div>
  <button id="savePassword">Změnit heslo</button>
  <details class="help"><summary>K čemu je heslo?</summary>
    <p>Stejné heslo chrání Wi-Fi přívěsku i tuhle aplikaci, aby duchovi nemohl velet někdo cizí.</p>
  </details>
  <div class="msg" id="pwMsg"></div>
</section>

<div class="grid2">
  <button id="reboot">🔄 Restart</button>
  <button class="danger" id="resetPet">🥚 Nový mazlíček</button>
</div>
</div>

<div class="toast" id="toast"></div>

<nav>
  <button data-tab="pet"><span>🐼</span>Můj duch</button>
  <button data-tab="scenes"><span>🎬</span>Scény</button>
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
const SCENE_ICONS = ['🪦', '🎋', '🏚️', '🫙', '❄️'];

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
  if (!$('tab-' + name)) name = 'pet';
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
  $(id + 'Bar').style.background = v < 25 ? 'var(--bad)' : v < 50 ? 'var(--warn)' : 'var(--mint)';
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
  $('hello').textContent = (s.greeting || 'Ahoj!') + ' 👋';
  $('fw').textContent = (s.soulsOnly ? s.deviceName : s.pet.name) + ' · verze ' + s.fw;
  $('fwText').textContent = s.fw;
  $('pwBanner').classList.toggle('hidden', !s.defaultPassword);
  const b = s.battery;
  $('batText').textContent = b.present ? b.percent + ' % (' + (b.mv / 1000).toFixed(2) + ' V)' : 'z USB kabelu';
  $('batChip').textContent = '🔋 ' + (b.present ? b.percent + ' %' : 'USB');
  $('apText').textContent = s.apSsid + (s.clients ? ' · připojeno ' + s.clients : '');
  $('apName').textContent = s.apSsid;
  const off = s.wifiOffIn >= 0 ? ' · vypnu za ' + Math.ceil(s.wifiOffIn / 60) + ' min' : '';
  $('netText').textContent = (s.online ? s.ssid + ' ✓' : 'nepřipojeno') + off;
  $('wifiChip').textContent = '📶 ' + (s.online ? 'online' : 'hotspot');

  // Pet
  const p = s.pet;
  $('petName').textContent = p.name;
  $('petMood').textContent = p.mood;
  $('petLevel').textContent = 'Level ' + p.level;
  $('petTitle').textContent = p.title;
  const span = p.xpNext ? p.xpNext - p.xpLevel : 1;
  $('xpBar').style.width = (p.xpNext ? Math.min(100, (p.xp - p.xpLevel) * 100 / span) : 100) + '%';
  $('xpText').textContent = p.xpNext ? 'Do dalšího levelu: ' + (p.xpNext - p.xp) + ' bodů péče ✨' : 'Nejvyšší level – král duchů! 👑';
  statBar('food', p.food); statBar('fun', p.fun); statBar('energy', p.energy);
  const sb = $('sleepBtn');
  sb.dataset.pet = p.asleep ? 'wake' : 'sleep';
  sb.innerHTML = p.asleep ? '<span>☀️</span>Probudit' : '<span>💤</span>Uspat';

  // Souls-only layout
  document.body.classList.toggle('souls-only', s.soulsOnly);
  $('soulsOnly').checked = s.soulsOnly;
  document.querySelector('nav button[data-tab=pet]').innerHTML = s.soulsOnly ? '<span>👻</span>Duše' : '<span>🐼</span>Můj duch';
  buttons('soulsTop', s.souls, i => api('/api/soul', { id: i }).then(refresh))
    .forEach((btn, i) => btn.classList.toggle('active', !s.autoShow && s.displayMode === 1 && i === s.soul));

  // Hunt
  const h = s.hunt;
  if (h) {
    $('huntFreed').textContent = h.freed ? '🕊️ ' + h.freed + '×' : '';
    $('huntRun').classList.toggle('hidden', !h.active);
    $('huntNew').classList.toggle('hidden', h.active);
    let dots = '';
    for (let i = 0; i < h.count; i++) dots += (h.mask >> i) & 1 ? '🔓' : '🔒';
    $('huntDots').textContent = dots;
    $('huntText').textContent = h.found + ' z ' + h.count + ' pečetí zlomeno' + (h.hints[0] ? ' · první nápověda: ' + h.hints[0] : '');
    const hh = $('huntHints'), n = Math.max(h.count || 6, 3);
    if (hh.children.length !== n + 1 && document.activeElement.tagName !== 'TEXTAREA') {
      let html = '';
      for (let i = 0; i <= n; i++) {
        html += '<label class="muted">' + (i === 0 ? 'Kde hledat 1. pečeť (ukáže se v aplikaci)' :
          i === n ? 'Po poslední pečeti (třeba vzkaz)' : 'Po pečeti ' + i + ' → kde je pečeť ' + (i + 1)) +
          '</label><textarea id="hint' + i + '" maxlength="120"></textarea>';
      }
      hh.innerHTML = html;
      h.hints.forEach((t, i) => { if ($('hint' + i)) $('hint' + i).value = t; });
    }
  }

  // Lovec světýlek
  const L = s.lights;
  if (L) {
    $('lightsBest').textContent = L.best ? '🏆 ' + L.best : '';
    $('lightsPlay').textContent = L.active ? '🏮 Hraje se…' : '▶ Hrát';
    $('lightsPlay').disabled = L.active;
    $('jarText').textContent = (L.jar % 100) + '/100' + (L.jar >= 100 ? ' · plných sklenic: ' + Math.floor(L.jar / 100) : '');
    $('jarBar').style.width = (L.jar % 100) + '%';
  }

  // Maze
  const g = s.game;
  if (g) {
    const total = g.stars.reduce((a, b) => a + b, 0);
    $('gameProgress').textContent = '⭐ ' + total + '/45';
    $('gamePlay').textContent = g.active ? '🎮 Hraje se level ' + g.level : '▶ Hrát level ' + g.unlocked;
    $('gamePlay').disabled = g.active;
    $('gameStop').classList.toggle('hidden', !g.active);
    const lv = $('levels');
    if (lv.children.length !== 15) {
      lv.innerHTML = '';
      for (let i = 1; i <= 15; i++) {
        const b = document.createElement('button');
        b.style.padding = '8px 2px';
        b.style.fontSize = '13px';
        b.onclick = () => gameCmd({ cmd: 'start', level: i });
        lv.appendChild(b);
      }
    }
    [...lv.children].forEach((b, i) => {
      const locked = i + 1 > g.unlocked;
      b.disabled = locked;
      b.innerHTML = '<b>' + (locked ? '🔒' : i + 1) + '</b><br>' + (g.stars[i] ? '⭐'.repeat(g.stars[i]) : '<span class="muted">·</span>');
      b.classList.toggle('active', g.active && g.level === i + 1);
    });
  }

  // Scenes
  const ghostMode = s.displayMode === 0;
  $('surpriseBtn').classList.toggle('active', s.autoShow);
  buttons('scenes', [...s.scenes.map((n, i) => (SCENE_ICONS[i] || '✨') + ' ' + n), '👤 Uvězněné duše'],
    i => api('/api/scene', { id: i }).then(refresh))
    .forEach((btn, i) => btn.classList.toggle('active', !s.autoShow && (ghostMode ? i === s.scene : i === s.scenes.length)));
  $('soulsBox').classList.toggle('hidden', ghostMode);
  buttons('souls', s.souls, i => api('/api/soul', { id: i }).then(refresh))
    .forEach((btn, i) => btn.classList.toggle('active', !ghostMode && i === s.soul));
  buttons('eyeColors', s.eyeColors, i => saveSettings({ eyeColor: i }).then(refresh))
    .forEach((btn, i) => btn.classList.toggle('active', i === s.eyeColor));
  document.querySelectorAll('#spooky button').forEach(btn => btn.classList.toggle('active', +btn.dataset.v === s.spooky));
  $('imuText').textContent = s.imu.ok ? (s.imu.calibrated ? 'funguje ✓' : 'funguje (nekalibrováno)') : 'nenalezen';
  $('leanDot').style.left = 'calc(' + (50 + s.imu.lean * 45) + '% - 7px)';
  $('tiltInvertBtn').classList.toggle('active', s.tiltInvert);

  $('displayOn').checked = s.display;
  $('qrOnBoot').checked = s.qrOnBoot;

  if (!formsFilled) {
    formsFilled = true;
    $('brightness').value = s.brightness;
    $('speed').value = s.frameDelay;
    $('speed2').value = s.frameDelay;
    $('sleepMinutes').value = s.sleepMinutes;
    $('wifiPolicy').value = s.wifiPolicy;
    $('wifiMinutes').value = s.wifiMinutes;
    $('hostname').value = s.hostname;
    $('manifestUrl').value = s.manifestUrl;
    $('motionSensitivity').value = s.motionSensitivity;
    $('petNameInput').value = p.name;
    $('greetingInput').value = s.greeting;
    $('subtitleInput').value = s.subtitle;
    $('deviceNameInput').value = s.deviceName;
    $('networks').innerHTML = s.networks.map((n, i) =>
      '<div class="grid2"><input id="ssid' + i + '" placeholder="Síť ' + (i + 1) + '" autocapitalize="off"><input id="pass' + i +
      '" type="password" placeholder="' + (n ? 'heslo beze změny' : 'heslo') + '"></div>').join('');
    s.networks.forEach((n, i) => $('ssid' + i).value = n);
    const base = 'http://' + s.hostname + '.local';
    $('exBoo').textContent = base + '/api/ghost?action=boo';
    $('exFeed').textContent = base + '/api/pet?cmd=feed&food=bamboo';
    $('exScene').textContent = base + '/api/scene?id=0';
    $('exBright').textContent = base + '/api/settings?brightness=50';
    $('authHeader').textContent = auth;
  }
  $('brightnessVal').textContent = $('brightness').value + ' %';
  $('speedVal').textContent = Math.round(1000 / $('speed').value) + ' snímků/s';
  $('speedVal2').textContent = Math.round(1000 / $('speed2').value) + ' snímků/s';
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

function gameCmd(params) { return api('/api/game', params).then(j => { toast(j.message); refresh(); }).catch(e => toast(e.message)); }
function saveSettings(params) { return api('/api/settings', params).catch(e => toast(e.message)); }
function petCmd(params) { return api('/api/pet', params).then(j => { toast(j.message); refresh(); }).catch(e => toast(e.message)); }

document.querySelectorAll('[data-food]').forEach(b => b.onclick = () => petCmd({ cmd: 'feed', food: b.dataset.food }));
document.querySelectorAll('[data-pet]').forEach(b => b.onclick = () => petCmd({ cmd: b.dataset.pet }));
document.querySelectorAll('[data-soul]').forEach(b => b.onclick = () => api('/api/soul', { action: b.dataset.soul }).catch(e => toast(e.message)));
document.querySelectorAll('[data-act]').forEach(b => b.onclick = () => api('/api/ghost', { action: b.dataset.act }).catch(e => toast(e.message)));
document.querySelectorAll('#spooky button').forEach(b => b.onclick = () => saveSettings({ spooky: b.dataset.v }).then(refresh));
$('surpriseBtn').onclick = () => saveSettings({ autoShow: state && state.autoShow ? 0 : 1 }).then(() => { toast(state && state.autoShow ? 'Překvapení vypnuto' : 'Překvapení zapnuto 🎲'); refresh(); });

$('soulsOnly').onchange = e => saveSettings({ soulsOnly: e.target.checked ? 1 : 0 }).then(refresh);
$('speed2').oninput = () => $('speedVal2').textContent = Math.round(1000 / $('speed2').value) + ' snímků/s';
$('speed2').onchange = () => saveSettings({ frameDelay: $('speed2').value });
function huntCmd(params) { return api('/api/hunt', params).then(j => { toast(j.message); refresh(); }).catch(e => toast(e.message)); }
$('huntStart').onclick = () => huntCmd({ cmd: 'new', count: $('huntCount').value });
$('huntStop').onclick = () => { if (confirm('Ukončit hru? Pečetě přestanou platit.')) huntCmd({ cmd: 'stop' }); };
$('huntPrint').onclick = () => window.open('/hunt/print', '_blank');
$('huntSaveHints').onclick = () => {
  const p = { cmd: 'hints' };
  document.querySelectorAll('#huntHints textarea').forEach(t => p['h' + t.id.slice(4)] = t.value);
  huntCmd(p);
};
$('lightsPlay').onclick = () => gameCmd({ cmd: 'lights' });
$('gamePlay').onclick = () => gameCmd({ cmd: 'start' });
$('gameStop').onclick = () => gameCmd({ cmd: 'stop' });
$('gameCalibrate').onclick = () => gameCmd({ cmd: 'calibrate' });
$('calibrate').onclick = () => api('/api/imu/calibrate', {}).then(() => { msg('imuMsg', 'Hotovo, poloha uložena ✓', 'ok'); refresh(); })
  .catch(e => msg('imuMsg', e.message, 'err'));
$('tiltInvertBtn').onclick = () => saveSettings({ tiltInvert: state.tiltInvert ? 0 : 1 }).then(refresh);
$('motionSensitivity').onchange = e => saveSettings({ motionSensitivity: e.target.value });

$('qrOnBoot').onchange = e => saveSettings({ qrOnBoot: e.target.checked ? 1 : 0 });
$('displayOn').onchange = e => api('/api/display', { on: e.target.checked ? 1 : 0 });
$('brightness').oninput = () => $('brightnessVal').textContent = $('brightness').value + ' %';
$('brightness').onchange = () => saveSettings({ brightness: $('brightness').value });
$('speed').oninput = () => $('speedVal').textContent = Math.round(1000 / $('speed').value) + ' snímků/s';
$('speed').onchange = () => saveSettings({ frameDelay: $('speed').value });
$('sleepMinutes').onchange = e => saveSettings({ sleepMinutes: e.target.value });
$('wifiPolicy').onchange = e => { $('wifiMinutesRow').classList.toggle('hidden', e.target.value !== '0'); saveSettings({ wifiPolicy: e.target.value }); };
$('wifiMinutes').onchange = e => saveSettings({ wifiMinutes: e.target.value });
$('saveManifest').onclick = () => saveSettings({ manifestUrl: $('manifestUrl').value }).then(() => msg('otaMsg', 'Uloženo', 'ok'));

$('saveMine').onclick = async () => {
  const renamed = $('deviceNameInput').value.trim() !== state.deviceName;
  try {
    await api('/api/settings', { greeting: $('greetingInput').value, subtitle: $('subtitleInput').value, deviceName: $('deviceNameInput').value });
    if ($('petNameInput').value.trim() && $('petNameInput').value.trim() !== state.pet.name) {
      await api('/api/pet', { cmd: 'name', name: $('petNameInput').value });
    }
    toast(renamed ? 'Uloženo! Wi-Fi se přejmenuje – připoj se pak znovu 📶' : 'Uloženo ✓');
    refresh();
  } catch (e) { toast(e.message); }
};
$('resetPet').onclick = () => { if (confirm('Opravdu začít s novým mazlíčkem? Level a body péče se smažou.')) petCmd({ cmd: 'reset' }); };

$('sleepNow').onclick = () => {
  if (confirm('Vypnout přívěsek? Zapneš ho vypínačem.')) api('/api/sleep', {}).then(() => toast('Dobrou noc 💤'));
};
$('reboot').onclick = () => { if (confirm('Restartovat přívěsek?')) api('/api/reboot', {}); };

$('saveWifi').onclick = async () => {
  const p = { hostname: $('hostname').value };
  state.networks.forEach((n, i) => { p['ssid' + i] = $('ssid' + i).value; p['pass' + i] = $('pass' + i).value; });
  try {
    await saveSettings({ hostname: p.hostname });
    await api('/api/wifi', p);
    msg('wifiMsg', 'Uloženo. Hotspot přívěsku poběží dál, domácí síť se připojí na pozadí.', 'ok');
  } catch (e) { msg('wifiMsg', e.message, 'err'); }
};

$('savePassword').onclick = async () => {
  const pw = $('newPassword').value;
  try { await api('/api/password', { password: pw }); setAuth(pw); formsFilled = false; $('newPassword').value = ''; msg('pwMsg', 'Heslo změněno. Wi-Fi přívěsku má teď nové heslo – připoj se znovu.', 'ok'); }
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
