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
    save: () => (T.isDisplay() ? null : { reboot: true }),
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
          <div class="row"><button type="button" class="btn" id="change-wifi">${T.icon('wifi')} Change WiFi network…</button><span class="hint">Restarts into the network chooser on the device's own access point; nothing else is erased.</span></div>` })}
        ${UI.card({ title: 'Restart', icon: 'rotate-ccw', body: html`<div class="row"><button type="button" class="btn" id="restart-btn">${T.icon('rotate-ccw')} Restart device</button><span class="hint">Keeps every setting. Handy after plugging in a new I²C sensor — they are detected at start-up.</span></div>` })}

        ${UI.card({ title: 'Reset everything', icon: 'trash-2', body: html`
          <p class="hint">A factory reset erases the settings, the WiFi credentials and the on-device data log. It is done on the device itself:</p>
          <ol class="small"><li>Press and release <strong>RESET</strong>.</li><li>Press and hold <strong>MODE</strong> for about 20 seconds until the LED turns ${T.led('red')}.</li><li>The device restarts with its own access point <code>TEHYBUG-…</code> (password <code>${T.AP_PASSWORD}</code>); join it and open <code>http://192.168.4.1/</code> to choose a WiFi network.</li></ol>` })}`;
    },
    mount(root) {
      root.addEventListener('click', e => {
        if (e.target.closest('#restart-btn')) restartDevice();
        if (e.target.closest('#change-wifi')) changeWifi();
        if (e.target.closest('#back-to-setup')) backToSetup();
      });
    },
    collect() {
      const p = T.radio('power');
      return { sleepModeActive: p === 'deep', lightSleepModeActive: p === 'light' };
    }
  });
})();
