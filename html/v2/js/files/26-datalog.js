/* Data log: the on-device log on the DS3231 + EEPROM module, offline mode,
 * the device clock, and reading the stored files. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI, $ = T.$;
  let lastLog = null; // the last /api/datalog answer

  function normaliseFrequency(v) { return Math.max(10, v); }

  function eepromCard(c) {
    return UI.card({ title: 'Log to the device', icon: 'hard-drive', body: html`
      ${UI.toggle({ id: 'eepromLogActive', cls: 'big', label: 'Write readings to the EEPROM', checked: !!c.eepromLogActive })}
      ${UI.field({ id: 'eepromLogFrequency', label: 'Every', labelHint: 'seconds', type: 'number', value: c.eepromLogFrequency || 60, attrs: 'min="60" inputmode="numeric"', hint: 'Timestamps have one-minute resolution, so 60 s or more. In offline mode this is also the sleep interval between wake-ups.' })}
      ${UI.field({ id: 'eepromLogMessage', label: 'Which values', labelHint: 'optional', value: c.eepromLogMessage, placeholder: 'empty = the default set of measured values', after: UI.fill('eepromLogMessage', 'log'), hint: html`Placeholders such as <code>%temp% %humi%</code>. Fewer values per line means more history fits.` })}
      ${UI.toggle({ id: 'eepromLogHourly', label: 'One file per hour (last 24 h) instead of per day (last month)', checked: !!c.eepromLogHourly, hint: 'Per day keeps a rolling month at day resolution; per hour keeps a rolling 24 hours at finer detail. Changing this erases the existing log.' })}
      ${UI.disclosure('How much fits?', html`
        <p class="hint">Each day (or hour) file holds about 2 KB on the 64 KB chip (FT24C512A, current modules) or 1 KB on the 32 KB chip (earlier modules) — the device detects which. A file that fills up wraps around and overwrites its own oldest lines, so the newest readings are always kept.</p>
        ${UI.table(['Values per line', 'Lines per file (64 / 32 KB)', 'Interval to cover a day'], [
          ['1', '~156 / ~77', '~9 / ~20 min'],
          ['2', '~106 / ~52', '~14 / ~28 min'],
          ['3', '~81 / ~40', '~18 / ~36 min'],
          ['4', '~65 / ~32', '~22 / ~45 min']
        ])}
        <p class="hint">Example: temperature + humidity every 30 minutes is ~48 lines a day — comfortably one file per day for a whole month.</p>`)}` });
  }

  function offlineCard(c) {
    const display = T.isDisplay();
    return UI.card({ title: 'Offline mode', icon: 'wifi-off', body: html`
      ${UI.toggle({ id: 'offlineModeActive', cls: 'big', label: 'Run with WiFi off', checked: !!c.offlineModeActive })}
      <p class="hint">${display
        ? 'The Display Weatherstation stays awake — screen, clock and alarms keep running — it just never connects to WiFi, and keeps logging if logging is on. The right button held for 10 seconds toggles the same thing on the device (the LED turns purple).'
        : 'The device never connects to WiFi: it wakes on the log interval, measures, appends one line to the log and deep-sleeps again. The lowest power draw there is, and no network needed.'}</p>
      ${UI.note('info', html`Offline mode is exclusive: saving it switches off every network destination, the sleep modes and setup mode, and turns logging on. <strong>This page is then unreachable</strong> — to read the log later, press RESET, then MODE until the LED turns blue, and come back here.`)}
      <div id="offline-unavailable" hidden>${UI.note('warn', 'No clock + EEPROM module was found, so offline mode would log nothing. If a DHT or DS18B20 is enabled on Port B (green) it occupies the pins the module needs — move it to Port A (black).')}</div>` });
  }

  function filesInner() {
    const d = lastLog;
    if (!d) return html`<p class="hint mb0">Checking for the clock + EEPROM module…</p>`;
    if (d.error) return html`<p class="hint mb0">Could not reach the device's data-log API.</p>`;
    if (!d.active) return html`<p class="hint mb0">No clock + EEPROM module detected. If one is attached and a DHT/DS18B20 is enabled on Port B (green), that sensor occupies the module's pins — move it to Port A on <a href="#/sensors">Sensors</a>.</p>`;
    const chip = d.capacity >= 65536 ? 'FT24C512A' : (d.capacity >= 32768 ? 'FT24C256A' : '');
    const files = d.files || [];
    return html`
      <div class="row">
        <div><strong>Device clock:</strong> ${d.timeSet ? d.time : html`<span class="badge warn">not set</span> <span class="hint">set it to start logging</span>`}</div>
        <button type="button" class="btn btn-sm" id="set-clock" data-nosave>${T.icon('clock')} Set from this browser</button>
        <button type="button" class="btn btn-sm" id="reload-log" data-nosave>${T.icon('refresh-cw')} Refresh</button>
      </div>
      ${d.capacity ? html`<p class="hint mt">Memory: ${Math.round(d.capacity / 1024)} KB${chip ? ' (' + chip + ')' : ''}, about ${d.slotBytes} bytes per file.</p>` : ''}
      ${files.length
        ? UI.table(['Date', 'File', 'Size', ''], files.map(f => [f.date || ('Day ' + String(f.name).replace('.txt', '')), html`<span class="hint">${f.name}</span>`, f.size + ' B', html`<button type="button" class="btn btn-sm" data-view="${f.name}" data-nosave>View</button>`]))
        : html`<div class="empty mt">No log files yet.</div>`}
      <div id="log-view" class="mt" hidden><div class="row"><strong id="log-view-name"></strong><span class="spacer"></span><button type="button" class="btn btn-sm" id="log-copy" data-nosave>${T.icon('copy')} Copy</button></div><pre id="log-view-content" class="mt"></pre></div>
      ${UI.disclosure('Reading a line', html`<p class="hint">Each line is the time of day followed by tagged values, e.g. <code>07:55 22.6t 48.3h 1013.2p</code>. Tags: <code>t</code> temperature, <code>t2</code> temperature 2, <code>h</code>/<code>h2</code> humidity, <code>p</code> pressure, <code>al</code> altitude, <code>l</code> light, <code>x</code> ADC, <code>q</code> IAQ, <code>c</code> eCO₂, <code>v</code> bVOC, <code>a</code> gas resistance. The date is the file. A custom “which values” template stores your own text instead.</p>`)}`;
  }

  async function loadLog() {
    try { lastLog = await T.Api.datalog(); } catch (e) { lastLog = { error: true }; }
    T.render($('#log-files'), filesInner());
    T.show('offline-unavailable', !!(lastLog && !lastLog.error && !lastLog.active));
  }
  async function setClock() {
    try { await T.Api.setTime(); T.Shell.toast('Clock set'); await loadLog(); }
    catch (e) { T.Shell.toast('Could not set the clock: ' + e.message, 'danger'); }
  }
  async function viewFile(name) {
    const box = $('#log-view');
    box.hidden = false;
    $('#log-view-name').textContent = name;
    $('#log-view-content').textContent = 'Loading…';
    try { const text = await T.Api.datalogFile(name); $('#log-view-content').textContent = text === '' ? '(empty file)' : text; }
    catch (e) { $('#log-view-content').textContent = 'Could not load the file: ' + e.message; }
  }

  T.definePage({
    id: 'datalog', title: 'Data log',
    nav: { group: 'more', icon: 'hard-drive', order: 1 },
    save: {
      reboot: true,
      confirm: async data => {
        const c = T.State.config;
        const goingOffline = data.offlineModeActive && !c.offlineModeActive;
        const layoutChange = c.eepromLogActive && data.eepromLogHourly !== !!c.eepromLogHourly;
        if (!goingOffline && !layoutChange) return true;
        return T.Shell.confirm({
          title: goingOffline ? 'Switch to offline mode?' : 'Change the log layout?',
          okLabel: goingOffline ? 'Go offline' : 'Change and erase',
          danger: !goingOffline,
          body: html`${goingOffline ? html`<p>After the restart the device runs with WiFi off and <strong>this page is unreachable</strong>. To get back: press RESET, then MODE until the LED turns blue${T.isDisplay() ? ', or hold the right button for 10 seconds' : ''}.</p>${lastLog && !lastLog.active ? UI.note('warn', 'No clock + EEPROM module was detected — the device would sleep and log nothing.') : ''}` : ''}
            ${layoutChange ? html`<p>Switching between per-day and per-hour files <strong>erases the existing log</strong>. Download anything you still need first.</p>` : ''}`
        });
      }
    },
    render() {
      const c = T.State.config;
      return html`${UI.pagehead('Data log', 'With a DS3231 clock + EEPROM module the device keeps timestamped readings on itself — no server, no network.')}
        ${T.isGeneric() ? UI.note('warn', 'The 1 MB build for first-generation boards has no data-log support; these settings have no effect on this device.') : ''}
        ${eepromCard(c)}
        ${offlineCard(c)}
        ${UI.card({ title: 'Stored data', icon: 'file-text', body: html`<div id="log-files">${filesInner()}</div>` })}`;
    },
    mount(root) {
      loadLog();
      root.addEventListener('click', e => {
        if (e.target.closest('#set-clock')) setClock();
        if (e.target.closest('#reload-log')) loadLog();
        const v = e.target.closest('[data-view]');
        if (v) viewFile(v.getAttribute('data-view'));
        if (e.target.closest('#log-copy')) T.copy($('#log-view-content').textContent).then(ok => T.Shell.toast(ok ? 'Copied' : 'Could not copy', ok ? '' : 'warn'));
      });
    },
    collect() {
      const offline = T.checked('offlineModeActive');
      const out = {
        eepromLogActive: T.checked('eepromLogActive'),
        eepromLogFrequency: normaliseFrequency(T.int('eepromLogFrequency', 60)),
        eepromLogHourly: T.checked('eepromLogHourly'),
        eepromLogMessage: T.val('eepromLogMessage').trim(),
        offlineModeActive: offline
      };
      if (offline) {
        Object.assign(out, {
          eepromLogActive: true, mqttActive: false, haActive: false, httpGetActive: false, httpPostActive: false,
          sleepModeActive: false, lightSleepModeActive: false, configModeActive: false
        });
      }
      return out;
    }
  });
})();
