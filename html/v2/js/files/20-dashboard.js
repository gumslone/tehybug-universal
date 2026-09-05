/* Dashboard: what the device is, where it stands in setup, what it reads,
 * and its log. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI, $ = T.$;
  const SENSOR_SWITCHES = ['dht_sensor', 'ds18b20_sensor', 'second_dht_sensor', 'second_ds18b20_sensor', 'adc_sensor'];

  function statusInner() {
    const i = T.State.info, c = T.State.config;
    const live = T.State.configLoaded && c.configModeActive === false;
    const q = parseInt(i.wifiQuality, 10);
    const key = i.key || c.key || '';
    return html`
      <div class="eyebrow">${T.boardName()}</div>
      <div class="status-mode ${live ? 'live' : 'setup'}">${T.icon(live ? 'radio' : 'settings')}${live ? 'Live — sending data' : 'Setup mode — sending is paused'}</div>
      <div class="kv-grid">
        <div><div class="k">Device key</div><div class="v"><code>${key || '…'}</code>${key ? html`<button type="button" class="icon-btn" data-copy="${key}" title="Copy key" aria-label="Copy key">${T.icon('copy')}</button>` : ''}</div></div>
        <div><div class="k">Firmware</div><div class="v">${i.gumboardVersion || '…'}<span class="hint">build ${i.fwBuild || '—'}</span></div></div>
        <div><div class="k">WiFi</div><div class="v">${i.wifiSSID || '…'}<span class="hint">${isFinite(q) ? q + '% · ' + i.wifiRSSI + ' dBm' : ''}</span></div></div>
        <div><div class="k">Address</div><div class="v">${i.ipAddress ? html`<a href="http://${i.ipAddress}/">${i.ipAddress}</a>` : '…'}${T.isGeneric() || !i.ipAddress ? '' : html`<span class="hint">or tehybug.local</span>`}</div></div>
      </div>
      ${UI.disclosure('More about this device', UI.kv([
        ['Chip ID', i.chipID],
        ['CPU', i.cpuFreqMHz ? i.cpuFreqMHz + ' MHz' : ''],
        ['Free memory', i.freeHeap ? T.fmt.bytes(i.freeHeap) : ''],
        ['Firmware size', i.sketchSize ? T.fmt.bytes(i.sketchSize) + ' used · ' + T.fmt.bytes(i.freeSketchSpace) + ' free for updates' : ''],
        ['Longest deep sleep', i.deepSleepMax ? T.fmt.secs(i.deepSleepMax) : ''],
        ['Up since restart', i.uptimeS != null ? T.fmt.duration(i.uptimeS) : ''],
        ['Web UI', T.UI_VERSION + (window.TEHYBUG_OFFLINE_UI ? ' (built-in copy)' : '') + (i.uiBuild ? ' · built-in copy ' + i.uiBuild : '')]
      ]))}`;
  }

  function stepsInner() {
    const c = T.State.config;
    const seen = T.Readings.known().filter(k => !/^cs2?$/.test(k)).length;
    // the generic build never reads the second port or the ADC, so a stale flag there is not "a sensor switched on"
    const sensorsOn = SENSOR_SWITCHES.filter(k => !T.isGeneric() || k === 'dht_sensor' || k === 'ds18b20_sensor').some(k => c[k]);
    const dest = T.destinations();
    const live = T.State.configLoaded && c.configModeActive === false;
    const steps = [
      { title: 'Sensors', done: seen > 0 || sensorsOn,
        text: seen ? seen + ' reading' + (seen === 1 ? '' : 's') + ' coming in' : (sensorsOn ? 'Sensors switched on — waiting for the first readings' : 'Switch on the sensors you attached (I²C sensors are found by themselves)'),
        action: html`<a class="btn btn-sm" href="#/sensors">Sensors ${T.icon('chevron-right')}</a>` },
      { title: 'Where the readings go', done: dest.length > 0,
        text: dest.length ? dest.map(d => d.label).join(' · ') : 'TeHyBug Cloud, Home Assistant, MQTT, your own server, or the on-device log',
        action: html`<a class="btn btn-sm" href="#/senddata">Send data ${T.icon('chevron-right')}</a>` },
      { title: 'Go live', done: live,
        text: live ? 'The device is live and sending' : 'Leaves setup mode and starts sending on schedule',
        action: live ? '' : html`<button type="button" class="btn btn-sm ${dest.length ? 'btn-primary' : ''}" data-golive>${T.icon('radio')} Go live</button>` }
    ];
    return html`<ol class="steps">${steps.map((s, i) => html`<li class="step ${s.done ? 'done' : ''}"><div class="num">${s.done ? T.icon('check') : i + 1}</div><div class="body"><div class="title">${s.title}</div><div class="text">${s.text}</div></div><div class="act">${s.action}</div></li>`)}</ol>`;
  }

  function readingsGrid() {
    const units = T.units();
    const keys = T.Readings.known();
    if (!keys.length) {
      return html`<div class="empty">${T.State.online ? 'No readings yet. Switch a sensor on under Sensors, or wait a moment — the device reads on connect.' : 'Waiting for the device…'}</div>`;
    }
    const at = T.State.sensorsAt ? T.fmt.time(new Date(T.State.sensorsAt)) : '';
    return html`<div class="readings">${keys.map(k => {
      const kk = T.Readings.keyFor(k, units);
      const text = T.Readings.isText(k);
      return html`<div class="reading"><div class="n">${T.icon(T.Readings.icon(k))}${T.Readings.name(k)}</div><div class="v ${text ? 'text' : ''}">${T.Readings.value(kk)}${text ? '' : html`<span class="u">${T.Readings.unit(kk)}</span>`}</div></div>`;
    })}</div>
    <div class="hint mt">Read at ${at}. Refreshed every 15 s while this page is open.</div>`;
  }

  function logLine(l) {
    return html`<div><span class="t">${T.fmt.time(l.t)}</span> <span class="f">${l.fn}:</span> ${l.message}</div>`;
  }
  function renderLog() {
    const box = $('#dash-log');
    if (!box) return;
    T.render(box, html`${T.State.log.map(logLine)}`);
    box.scrollTop = box.scrollHeight;
  }
  function appendLog(line) {
    const box = $('#dash-log');
    if (!box) return;
    const stick = box.scrollHeight - box.scrollTop - box.clientHeight < 40;
    const div = document.createElement('div');
    T.render(div, logLine(line));
    box.appendChild(div.firstChild);
    if (stick) box.scrollTop = box.scrollHeight;
  }

  let pollTimer = null;
  let reading = false;
  async function refresh(manual) {
    if (reading) return;
    if (!T.State.online && !manual) return;
    reading = true;
    const btn = $('#refresh-btn');
    if (btn) btn.disabled = true;
    try { T.applySensors(await T.Api.sensor()); }
    catch (e) { if (manual) T.Shell.toast('Could not read the sensors: ' + e.message, 'warn'); }
    finally { reading = false; if (btn) btn.disabled = false; }
  }

  T.definePage({
    id: 'dashboard', title: 'Dashboard',
    nav: { group: 'top', icon: 'home', order: 0 },
    render() {
      return html`${UI.pagehead('Dashboard')}
        ${UI.card({ id: 'dash-status', body: statusInner() })}
        ${UI.card({ title: 'Set-up progress', icon: 'zap', body: html`<div id="dash-steps">${stepsInner()}</div>` })}
        ${UI.card({ title: 'Readings', icon: 'activity', actions: html`${UI.unitsSeg()}<button type="button" class="icon-btn" id="refresh-btn" title="Read the sensors now" aria-label="Refresh" data-nosave>${T.icon('refresh-cw')}</button>`, body: html`<div id="dash-readings">${readingsGrid()}</div>` })}
        ${UI.card({ title: 'Device log', icon: 'terminal', actions: html`<button type="button" class="btn btn-sm" id="log-clear" data-nosave>Clear</button>`, body: html`<div class="log" id="dash-log"></div><div class="hint mt">What the device is doing right now — connection attempts, sends, errors. Useful when something does not arrive where it should.</div>` })}
        ${UI.card({ title: 'REST API', icon: 'database', body: html`<p class="hint">The device answers JSON on its own address, for scripts and other integrations:</p>
          <ul class="small"><li><a href="/api/info" target="_blank" rel="noopener">/api/info</a> — device and system information</li><li><a href="/api/sensor" target="_blank" rel="noopener">/api/sensor</a> — reads the sensors and returns the values</li><li><a href="/api/config" target="_blank" rel="noopener">/api/config</a> — the configuration (POST JSON to it to change settings)</li><li><a href="/api/datalog" target="_blank" rel="noopener">/api/datalog</a> — the on-device data log, if a module is attached</li></ul>` })}`;
    },
    mount(root) {
      renderLog();
      pollTimer = setInterval(() => refresh(false), 15000);
      // the websocket triggers a read on connect; if nothing came, ask
      setTimeout(() => { if (pollTimer && !T.Readings.known().length) refresh(false); }, 3000);
      root.addEventListener('click', e => {
        if (e.target.closest('#refresh-btn')) refresh(true);
        if (e.target.closest('#log-clear')) { T.State.log.length = 0; renderLog(); }
      });
    },
    unmount() { clearInterval(pollTimer); pollTimer = null; },
    on: {
      info() { T.render($('#dash-status'), statusInner()); },
      config() { T.render($('#dash-status'), statusInner()); T.render($('#dash-steps'), stepsInner()); },
      sensors() { T.render($('#dash-readings'), readingsGrid()); T.render($('#dash-steps'), stepsInner()); },
      units() { T.render($('#dash-readings'), readingsGrid()); },
      log(line) { appendLog(line); },
      online() { T.render($('#dash-readings'), readingsGrid()); },
      offline() { T.render($('#dash-readings'), readingsGrid()); }
    }
  });
})();
