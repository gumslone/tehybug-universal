/* Get started: the three steps, explained, with live status. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI;

  T.definePage({
    id: 'start', title: 'Get started',
    nav: { group: 'setup', icon: 'book-open', order: 0 },
    render() {
      const c = T.State.config, i = T.State.info;
      const display = T.isDisplay();
      const dest = T.destinations();
      const live = T.State.configLoaded && c.configModeActive === false;
      const key = i.key || c.key || '';
      return html`${UI.pagehead('Get started', 'From a fresh TeHyBug to readings arriving somewhere, in three steps.')}
        ${UI.card({ body: html`
          <p>You are looking at the device's <strong>setup mode</strong>: WiFi on, sending paused, this page served. Everything is configured here, then the device goes live and runs on its own.</p>
          <div class="kv-grid"><div><div class="k">Your device key</div><div class="v"><code>${key || '…'}</code>${key ? html`<button type="button" class="icon-btn" data-copy="${key}" aria-label="Copy key">${T.icon('copy')}</button>` : ''}</div></div>
          <div><div class="k">Board</div><div class="v">${T.boardName()}</div></div></div>
          <p class="hint mt">The key identifies this device on tehybug.com and in every payload (<code>%key%</code>).</p>` })}

        ${UI.card({ title: 'Step 1 — Sensors', icon: 'thermometer', body: html`
          <p>I²C sensors (BME280/BMP280, BME680, AHT20, AM2320, MAX44009, SGP30) are detected automatically. Sensors on the jack ports have to be switched on, because a sensor that is enabled but not attached can make the device restart in a loop.</p>
          <ul>
            <li><strong>Port A (black):</strong> one DHT, DS18B20 or an analog sensor (ADC, e.g. soil moisture). Readings <code>%temp2%</code> / <code>%humi2%</code> / <code>%adc%</code>.</li>
            ${display ? '' : html`<li><strong>Port B (green):</strong> one DHT or DS18B20. Readings <code>%temp%</code> / <code>%humi%</code>. This port shares the I²C pins, so while it is in use I²C sensors and the clock/data-log module are not available.</li>`}
          </ul>
          ${UI.note('info', 'BME680 air quality (IAQ, eCO₂, bVOC) needs the sensor to run continuously for 30+ minutes to calibrate, so it does not work with deep sleep. Temperature, humidity and pressure do.')}
          <div class="row"><a class="btn btn-primary" href="#/sensors">${T.icon('sliders')} Open Sensors</a><span class="hint">Then check the readings on the <a href="#/dashboard">dashboard</a>.</span></div>` })}

        ${UI.card({ title: 'Step 2 — Where the readings go', icon: 'send', body: html`
          <p>Pick one — or combine MQTT and HTTP if you like:</p>
          <ul>
            <li><strong>TeHyBug Cloud</strong> — the easiest: charts, history and alerts on tehybug.com, nothing to host. Just your device key.</li>
            <li><strong>Home Assistant</strong> — the sensors appear by themselves through MQTT auto-discovery.</li>
            <li><strong>MQTT</strong> — any broker, your topic and payload.</li>
            <li><strong>HTTP GET / POST</strong> — your own server or a service such as ThingSpeak; <code>%placeholders%</code> carry the values.</li>
            <li><strong>On-device data log</strong> — with the DS3231 clock + EEPROM module, readings are stored on the device itself, even with WiFi off.</li>
          </ul>
          ${dest.length ? UI.note('ok', html`Currently set: ${dest.map(d => d.label).join(', ')}.`) : ''}
          <div class="row"><a class="btn btn-primary" href="#/senddata">${T.icon('send')} Open Send data</a><a class="btn" href="#/datalog">${T.icon('hard-drive')} Data log</a></div>` })}

        ${UI.card({ title: 'Step 3 — Go live', icon: 'radio', body: html`
          ${display
            ? html`<p>Going live starts the sending. On the Display Weatherstation everything else — screen, clock, alarms and this web interface — keeps running in every mode; only the TEHYBUG setup access point disappears.</p>`
            : html`<p>Going live ends setup mode: the device restarts, starts sending on schedule and, depending on the power mode, sleeps in between. <strong>This web interface is then no longer served</strong> — that is by design, it is what saves the battery.</p>
              <ul><li><strong>Deep sleep</strong> for battery: months of runtime, unreachable while asleep.</li><li><strong>Light sleep</strong>: sleeps but keeps WiFi, wakes fast.</li><li><strong>Always on</strong> for USB/mains and for BME680 air quality.</li></ul>`}
          ${live ? UI.note('ok', 'The device is live.') : html`<div class="row"><button type="button" class="btn btn-primary" data-golive>${T.icon('radio')} Go live</button><span class="hint">You choose the power mode in the next step.</span></div>`}` })}

        ${UI.card({ title: 'Getting back into setup', icon: 'settings', body: html`
          <ol>
            <li>Press and release <strong>RESET</strong>.</li>
            <li>Within about a second, press <strong>MODE</strong>. The LED turns <strong>blue</strong> — setup mode is on and this page is served again.</li>
            <li>If the LED stays dark, press RESET twice about a second apart, then MODE (a device that had deep-slept needs the second reset).</li>
          </ol>
          ${UI.note('warn', html`Do not hold MODE <em>while</em> pressing RESET: MODE sits on the chip's boot pin, and holding it through a reset starts the firmware-flashing mode instead. Press RESET first, let go, then MODE.`)}
          <p class="hint">A device with nothing configured to send always starts setup mode by itself, so you cannot lock yourself out. On its own access point (<code>TEHYBUG-…</code>, password <code>${T.AP_PASSWORD}</code>) the page at <code>192.168.4.1</code> shows the address to use on your network.</p>` })}`;
    }
  });
})();
