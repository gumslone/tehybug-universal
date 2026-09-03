/* Help: troubleshooting, the buttons and LED, every placeholder. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI;

  const faq = (q, a) => UI.disclosure(q, a, false, 'faq');

  T.definePage({
    id: 'help', title: 'Help',
    nav: { group: 'more', icon: 'help-circle', order: 4 },
    render() {
      const display = T.isDisplay();
      return html`${UI.pagehead('Help')}
        ${UI.card({ title: 'Troubleshooting', icon: 'help-circle', body: html`
          ${faq('How do I get back into setup mode?', html`<ol><li>Press and release <strong>RESET</strong>.</li><li>Within about a second press <strong>MODE</strong>; the LED turns ${T.led('blue')}.</li><li>If the LED stays dark, press RESET twice about a second apart, then MODE.</li></ol><p class="hint">Never hold MODE while pressing RESET — that starts firmware-flashing mode and the firmware does not run. If the device looks stuck, press RESET alone.</p>`)}
          ${faq('The device does not connect to WiFi', html`<ul><li>Factory reset: RESET, then hold MODE ~20 s until the LED turns ${T.led('red')}.</li><li>Join the <code>TEHYBUG-…</code> access point (password <code>${T.AP_PASSWORD}</code>) and open <code>http://192.168.4.1/</code> to pick a network.</li><li>2.4 GHz only — the ESP8266 cannot see 5 GHz networks.</li></ul>`)}
          ${faq('I cannot open this page', html`<ul><li>The device must be in setup mode (${T.led('blue')} LED)${display ? ' — on the Display Weatherstation the page is also served in live mode' : ''}.</li><li>Try the IP address instead of <code>tehybug.local</code> (first-generation boards have no mDNS).</li><li>While it deep-sleeps the device is off the network entirely.</li><li>Interface not loading but the address shows? You are probably still on the TEHYBUG access point, which has no internet — rejoin your WiFi.</li></ul>`)}
          ${faq('Readings are missing or wrong', html`<ul><li>Check the sensor sits in the right port and is switched on under <a href="#/sensors">Sensors</a>.</li><li>Port B (green) and the I²C bus share pins: a DHT/DS18B20 there hides I²C sensors and the clock module.</li><li>Use the calibration offsets for a constant error.</li><li>BME680 air quality needs 30+ minutes of continuous running (no deep sleep) to calibrate; IAQ accuracy reaches 3 when done. Its eCO₂ and bVOC are estimates from VOC, not CO₂ measurements.</li></ul>`)}
          ${faq('Nothing arrives at my server / broker / Home Assistant', html`<ul><li>Has the device gone live? Sending is paused in setup mode.</li><li>Watch the device log on the <a href="#/dashboard">dashboard</a> while it tries — it prints connection errors.</li><li>Test the URL from a browser; check broker address, port, user and password (2.4 GHz WiFi, same network).</li><li>With deep sleep the first send comes after the first interval.</li><li>In Home Assistant: the Mosquitto add-on running, the MQTT integration enabled, the user exists — sensors appear after the first send under Settings → Devices &amp; services → MQTT.</li></ul>`)}
          ${faq('The device keeps restarting', html`<ul><li>A sensor is switched on but not attached — switch it off (or attach it).</li><li>Weak power supply: a good USB adapter (≥500 mA) or a charged battery.</li><li>Very short intervals (&lt;30 s) plus many destinations can starve the device; 60 s or more.</li></ul>`)}
          ${faq('Can I log without any network?', html`<p>Yes — with the DS3231 clock + EEPROM module attached, readings are stored on the device. Set it up on <a href="#/datalog">Data log</a>; offline mode keeps WiFi off entirely for the lowest power draw. Not available on first-generation (1 MB) boards.</p>`)}
          ${faq('Do I need to update the firmware?', html`<p>Only if you miss a feature or hit a problem. A working device stays working; every update can change something you rely on. See <a href="#/firmware">Firmware</a>.</p>`)}` })}

        ${UI.card({ title: 'Buttons & LED', icon: 'cpu', body: html`
          ${UI.table(['', 'Action', 'Effect'], [
            [html`<strong>RESET</strong>`, 'press', 'Restart'],
            [html`<strong>MODE</strong>`, 'press within 1 s after RESET', html`Setup mode — LED ${T.led('blue')}`],
            [html`<strong>MODE</strong>`, 'hold ~20 s after RESET', html`Factory reset — LED ${T.led('red')}`],
            ...(display ? [[html`<strong>Left / Right</strong>`, 'click', 'Switch screen page; mute an alarm'], [html`<strong>Right</strong>`, 'hold 10 s', html`Toggle offline mode — LED ${T.led('purple')}`]] : [])
          ])}
          <p class="hint">MODE is on the chip's boot pin: pressing it after RESET is fine; holding it while RESET is pressed starts flashing mode instead.</p>` })}

        ${UI.card({ title: 'All placeholders', icon: 'list', body: html`
          <p class="hint">Any of these can be used in URLs, payloads, the data-log template${display ? ' and the screen lines' : ''} (not in the MQTT topic, which is sent as written). Which ones carry values depends on the attached sensors — the ones this device produces are listed on <a href="#/senddata">Send data</a>.</p>
          <div class="ph-list">${T.Readings.order.map(k => html`<div><code>%${k}%</code><span>${T.Readings.name(k)}</span><span class="u">${T.Readings.unit(k)}</span></div>`)}
            <div><code>%temp_imp%</code><span>Temperature</span><span class="u">°F</span></div>
            <div><code>%key%</code><span>Device key</span><span class="u"></span></div></div>
          <p class="hint mt">Fahrenheit twins exist for temperature, dew point and heat index (<code>%temp_imp%</code>, <code>%dew_imp%</code>, <code>%hi_imp%</code>; second sensor: <code>%temp2_imp%</code>, <code>%dew_imp2%</code>, <code>%hi_imp2%</code>).</p>` })}

        ${UI.card({ title: 'More', icon: 'globe', body: html`<ul class="mb0"><li><a href="https://tehybug.com" target="_blank" rel="noopener">tehybug.com</a> — the cloud service, documentation and support</li><li><a href="${T.REPO}" target="_blank" rel="noopener">GitHub</a> — source code, firmware releases, issues</li><li><a href="https://www.tindie.com/stores/gumslone/" target="_blank" rel="noopener">Tindie</a> — hardware</li></ul>` })}`;
    }
  });
})();
