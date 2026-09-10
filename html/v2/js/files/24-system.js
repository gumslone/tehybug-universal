/* Power & go live: the mode the device runs in, the switch that starts
 * sending, and a restart. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI, $ = T.$;

  async function restartDevice() {
    const ok = await T.Shell.confirm({ title: 'Restart the device?', body: 'Settings are kept. It is back in about 15 seconds.', okLabel: 'Restart' });
    if (!ok) return;
    try { await T.Api.saveConfig({ reboot: true }); await T.Restart.wait({ text: 'The device is restarting.' }); }
    catch (e) { T.Shell.toast('Could not restart: ' + e.message, 'danger'); }
  }
  async function changeWifi() {
    const i = T.State.info;
    const ap = i.apSsid || 'TEHYBUG-…';
    const ok = await T.Shell.confirm({
      title: 'Change the WiFi network?',
      okLabel: 'Restart into WiFi setup',
      body: html`<p>The device restarts and opens its own WiFi network <strong>${ap}</strong> (password <code>${T.AP_PASSWORD}</code>) with the network chooser, instead of joining <strong>${i.wifiSSID || 'the saved network'}</strong>.</p>
        <p>Nothing is erased: the saved network stays until you save a new one, and <em>Exit</em> in the chooser reconnects to it. All other settings are kept.</p>`
    });
    if (!ok) return;
    try {
      await T.Api.wifiPortal();
    } catch (e) { T.Shell.toast('Could not restart: ' + e.message, 'danger', 6000); return; }
    T.Live.stop();
    T.Shell.deviceGone = 'Device in WiFi setup';
    T.Bus.emit('offline');
    T.Shell.dialog({
      title: 'Now join the device\'s WiFi',
      body: html`<ol><li>On your phone or computer, join the WiFi network <strong>${ap}</strong> — password <code>${T.AP_PASSWORD}</code>.</li><li>Open <a href="http://192.168.4.1/">http://192.168.4.1/</a> and pick the new network.</li><li>After saving, the page hands you over to the configuration by itself. To keep the old network instead, choose <em>Exit</em>.</li></ol>
        <p class="hint">This page will not reconnect on its own: the device is on its own network now.</p>`,
      buttons: [{ label: 'Close' }]
    });
  }
  // Settings as one JSON file: everything the device stores except what is
  // this device's own (its key), transient (setup mode) or secret (the MQTT
  // password, which the device never reveals), so the file can be kept or
  // loaded onto another TeHyBug.
  const NOT_EXPORTED = ['key', 'configModeActive', 'reboot', 'mqttPassword'];
  const notLoaded = () => { if (T.State.configLoaded) return false; T.Shell.toast('The settings have not loaded from the device yet — try again in a moment', 'warn', 5000); return true; };
  function exportSettings() {
    if (notLoaded()) return;
    const c = T.State.config, i = T.State.info;
    const out = { _tehybug: { firmware: i.gumboardVersion || '', board: T.board(), exported: new Date().toISOString(), note: 'TeHyBug settings backup - load it on Power & go live' } };
    Object.keys(c).forEach(k => { if (NOT_EXPORTED.indexOf(k) < 0) out[k] = c[k]; });
    T.saveAs('tehybug-' + T.hostLabel(c.deviceName) + '-settings.json', JSON.stringify(out, null, 2), 'application/json');
    T.Shell.toast('Settings saved as a file' + (c.mqttPassword === '********' ? ' - the MQTT password is not in it' : ''));
  }
  async function importSettings(file) {
    if (notLoaded()) return;
    let data;
    try { data = JSON.parse(await file.text()); } catch (e) { T.Shell.toast('This is not a settings file', 'danger'); return; }
    if (!data || typeof data !== 'object' || Array.isArray(data)) { T.Shell.toast('This is not a settings file', 'danger'); return; }
    const known = Object.keys(T.State.config);
    // a password is applied only when the file really carries one (typed
    // into the file by hand): an empty or masked one must not clear the
    // device's
    const keys = Object.keys(data).filter(k => k[0] !== '_' && (k === 'mqttPassword' ? !!data[k] && data[k] !== '********' : NOT_EXPORTED.indexOf(k) < 0));
    const usable = keys.filter(k => known.indexOf(k) >= 0);
    if (!usable.length) { T.Shell.toast('No TeHyBug settings found in this file', 'danger'); return; }
    const meta = data._tehybug || {};
    const ok = await T.Shell.confirm({
      title: 'Restore these settings?',
      okLabel: 'Restore and restart',
      body: html`<p><strong>${file.name}</strong>${meta.exported ? html` <span class="hint">saved ${meta.exported.slice(0, 10)}${meta.board ? ' from a ' + meta.board + ' board' : ''}</span>` : ''}</p>
        <p>${usable.length} settings replace what is on this device; the device key and WiFi stay as they are. The device restarts afterwards.</p>
        ${data.deviceName ? html`<p class="hint">Device name in the file: <strong>${data.deviceName}</strong> — change it afterwards if this is a second device.</p>` : ''}
        ${meta.board && meta.board !== T.board() ? UI.note('warn', 'The file comes from a different board type; settings that do not apply here are ignored.') : ''}`
    });
    if (!ok) return;
    const payload = {};
    usable.forEach(k => { payload[k] = data[k]; });
    try {
      await T.Api.saveConfig(Object.assign({ reboot: true }, payload));
      T.applyConfig(payload);
      await T.Restart.wait({ text: 'The settings are stored and the device is restarting.' });
    } catch (e) { T.Shell.toast('Not restored: ' + e.message, 'danger', 6000); }
  }
  async function backToSetup() {
    try {
      await T.Api.saveConfig({ configModeActive: true, reboot: true });
      T.applyConfig({ configModeActive: true });
      await T.Restart.wait({ title: 'Back to setup mode…', text: 'The device is restarting into setup mode; sending pauses.' });
    } catch (e) { T.Shell.toast('Could not switch: ' + e.message, 'danger'); }
  }

  T.definePage({
    id: 'system', title: 'Power & go live',
    nav: { group: 'setup', icon: 'power', order: 3 },
    save: { reboot: true },
    render() {
      const c = T.State.config, i = T.State.info;
      const display = T.isDisplay();
      const live = T.State.configLoaded && c.configModeActive === false;
      const power = c.sleepModeActive ? 'deep' : (c.lightSleepModeActive ? 'light' : 'on');
      const dest = T.destinations();
      const maxSleep = i.deepSleepMax ? T.fmt.secs(i.deepSleepMax) : '';
      return html`${UI.pagehead('Power & go live')}
        ${UI.card({ title: live ? 'Live' : 'Setup mode', icon: live ? 'radio' : 'settings', body: html`
          ${live
            ? html`<p>The device is live: it sends on schedule${display ? ' while the screen and clock keep running' : ''}.</p>
              ${display ? html`<div class="row"><button type="button" class="btn" id="back-to-setup">${T.icon('settings')} Back to setup mode</button><span class="hint">Pauses sending and brings the setup access point back.</span></div>` : ''}`
            : html`<p>Sending is paused while you set things up. ${dest.length ? html`Ready to go: <strong>${dest.map(d => d.label).join(', ')}</strong>.` : html`No destination is switched on yet — see <a href="#/senddata">Send data</a>.`}</p>
              <div class="row"><button type="button" class="btn btn-primary" data-golive ${dest.length ? '' : 'disabled'}>${T.icon('radio')} Go live</button>
              <span class="hint">${dest.length ? (display ? 'The screen, clock, alarms and this page keep running.' : 'This page stops being served; RESET then MODE brings it back.') : html`Switch on a destination on <a href="#/senddata">Send data</a> first.`}</span></div>`}` })}

        ${display
          ? UI.card({ title: 'Power', icon: 'battery', body: html`<p class="hint mb0">The Display Weatherstation is mains powered and its screen has to keep drawing, so it has no sleep modes. To run it without WiFi, use offline mode on <a href="#/datalog">Data log</a> or hold the right button for 10 seconds.</p>` })
          : UI.card({ title: 'Power', icon: 'battery', body: html`
            ${UI.choice({ name: 'power', value: power, options: [
              { value: 'deep', label: 'Deep sleep — for battery', hint: 'Powers down between sends (≈20 µA), wakes, connects, sends, sleeps again. Unreachable while asleep' + (maxSleep ? '; longest interval ' + maxSleep : '') + '.' },
              { value: 'light', label: 'Light sleep', hint: 'CPU and radio power down between sends; on each wake the device rejoins the network (well under a second on a good signal) and sends. Far less power than always on, more than deep sleep.' },
              { value: 'on', label: 'Always on — USB or mains', hint: 'WiFi stays connected all the time (≈80 mA). Required for BME680 air quality, which needs 30+ minutes of continuous running.' }
            ] })}
            ${UI.disclosure('How long does a battery last?', html`
              ${UI.table(['Mode', 'Interval', '2000 mAh battery'], [
                ['Always on', 'any', '~1 day'],
                ['Light sleep', '5 min', '~2 weeks (rough)'],
                ['Deep sleep', '15 min', '~3–5 months'],
                ['Deep sleep', '1 h', '~6–12 months']
              ])}
              <p class="hint">Rough figures; a weak WiFi signal (long connects) and cold weather shorten them. The interval is set per destination on <a href="#/senddata">Send data</a>.</p>`)}` })}

        ${UI.card({ title: 'WiFi network', icon: 'wifi', body: html`
          <p class="hint">Connected to <strong>${i.wifiSSID || '…'}</strong>${i.ipAddress ? html` as <strong>${i.ipAddress}</strong>` : ''}.</p>
          ${T.isGeneric() ? '' : UI.field({ id: 'deviceName', label: 'Device name', labelHint: 'its address on your network', value: c.deviceName || '', placeholder: 'TeHyBug', attrs: 'maxlength="32" autocomplete="off"',
            after: html`<div class="hint">Reachable at <code id="mdns-preview">http://${T.hostLabel(c.deviceName)}.local/</code>. Letters, digits and hyphens; give each device its own name if you have several. The router's client list shows it too.</div>` })}
          <div class="row"><button type="button" class="btn" id="change-wifi">${T.icon('wifi')} Change WiFi network…</button><span class="hint">Restarts into the network chooser on the device's own access point; nothing else is erased.</span></div>` })}
        ${UI.card({ title: 'WiFi radio', icon: 'radio', body: html`
          <p class="hint">${i.wifiRSSI ? html`Signal <strong>${i.wifiRSSI} dBm</strong>${i.wifiTxDbm ? html`, sending at <strong>${i.wifiTxDbm} dBm</strong>` : ''}. ` : ''}The chip's defaults suit most homes; these help at the edge of range, or on a battery or supply that sags during the send bursts.</p>
          <h3>Transmit power</h3>
          ${UI.choice({ name: 'wifiPower', value: c.wifiPower || 'max', options: [
            { value: 'max', label: 'Maximum', hint: '20.5 dBm, the chip\'s default. The best reach.' },
            { value: 'auto', label: 'Automatic', hint: 'Less power while the signal is strong — down to 10 dBm at −55 dBm or better. Easier on a tired battery: the transmit bursts are what dips its voltage. Back to maximum whenever a connect fails.' },
            { value: 'low', label: 'Low', hint: '10 dBm. For a device right next to the router, or a supply that cannot deliver the bursts.' }
          ] })}
          <h3 class="mt">Mode</h3>
          ${UI.choice({ name: 'wifiMode', value: c.wifiMode || 'n', options: [
            { value: 'n', label: 'Standard (802.11n)', hint: 'The chip\'s default.' },
            { value: 'g', label: 'Compatible (802.11g)', hint: 'Slower rates that reach a little further; every router accepts them.' },
            { value: 'b', label: 'Long range (802.11b)', hint: 'The most sensitive receiver mode, for a device at the edge of range. Some routers and mesh systems have these legacy rates switched off — then the device cannot join at all and opens its setup access point, where you can switch back.' }
          ] })}` })}
        ${UI.card({ title: 'Backup', icon: 'save', body: html`
          <p class="hint">All settings as one file: to keep, or to load onto another TeHyBug. WiFi credentials and the device key are not part of it.</p>
          <div class="row">
            <button type="button" class="btn" id="cfg-export" data-nosave>${T.icon('download')} Download settings</button>
            <span class="filepick slim" data-nosave><input type="file" id="cfg-file" accept=".json,application/json"><label for="cfg-file" class="btn">${T.icon('upload')} Restore from file…</label></span>
          </div>` })}
        ${UI.card({ title: 'Restart', icon: 'rotate-ccw', body: html`<div class="row"><button type="button" class="btn" id="restart-btn">${T.icon('rotate-ccw')} Restart device</button><span class="hint">Keeps every setting. Handy after plugging in a new I²C sensor — they are detected at start-up.</span></div>` })}

        ${UI.card({ title: 'Reset everything', icon: 'trash-2', body: html`
          <p class="hint">A factory reset erases the settings, the WiFi credentials and the on-device data log. It is done on the device itself:</p>
          <ol class="small"><li>Press and release <strong>RESET</strong>.</li><li>Press and hold <strong>MODE</strong> for about 20 seconds until the LED turns ${T.led('red')}.</li><li>The device restarts with its own access point <code>TEHYBUG-…</code> (password <code>${T.AP_PASSWORD}</code>); join it and open <code>http://192.168.4.1/</code> to choose a WiFi network.</li></ol>` })}`;
    },
    mount(root) {
      root.addEventListener('input', e => {
        if (e.target.id !== 'deviceName') return;
        const p = $('#mdns-preview');
        if (p) p.textContent = 'http://' + T.hostLabel(e.target.value) + '.local/';
      });
      root.addEventListener('change', e => {
        if (e.target.id !== 'cfg-file') return;
        const f = e.target.files && e.target.files[0];
        e.target.value = '';
        if (f) importSettings(f);
      });
      root.addEventListener('click', e => {
        if (e.target.closest('#cfg-export')) exportSettings();
        if (e.target.closest('#restart-btn')) restartDevice();
        if (e.target.closest('#change-wifi')) changeWifi();
        if (e.target.closest('#back-to-setup')) backToSetup();
      });
    },
    collect() {
      const out = {};
      if (!T.isGeneric()) out.deviceName = T.val('deviceName').trim();
      out.wifiPower = T.radio('wifiPower') || 'max';
      out.wifiMode = T.radio('wifiMode') || 'n';
      if (!T.isDisplay()) { const p = T.radio('power'); out.sleepModeActive = p === 'deep'; out.lightSleepModeActive = p === 'light'; }
      return out;
    }
  });
})();
