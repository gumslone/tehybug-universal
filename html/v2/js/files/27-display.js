/* Display & alarms — the Display Weatherstation's screen lines, clock,
 * night mode and three alarms. Saved without a restart: the firmware reads
 * these settings live, so the screen follows within a second. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI, $ = T.$;
  const DAYS = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
  const hhmm = v => { const m = /^(\d{1,2}):(\d{2})$/.exec(String(v || '').trim()); return m ? (m[1].length === 1 ? '0' : '') + m[1] + ':' + m[2] : ''; };
  let clockTimer = null;

  function alarmCard(n, c) {
    const p = 'alarm' + n;
    const csv = String(c[p + 'Weekdays'] || '0,0,0,0,0,0,0').split(',');
    return UI.card({ title: 'Alarm ' + n, icon: 'bell', body: html`
      ${UI.toggle({ id: p + 'Active', label: 'Enabled', checked: !!c[p + 'Active'] })}
      <div class="fields-inline mt">
        ${UI.field({ id: p + 'Time', label: 'Time', type: 'time', value: hhmm(c[p + 'Time']) })}
        ${UI.field({ id: p + 'Message', label: 'Message', labelHint: 'shown on the screen', value: c[p + 'Message'], placeholder: 'optional' })}
      </div>
      <div class="field"><label>Days</label><div class="weekdays">${DAYS.map((d, i) => html`<label class="${csv[i] === '1' ? 'on' : ''}"><input type="checkbox" data-alarm="${n}" data-day="${i}" ${csv[i] === '1' ? 'checked' : ''}>${d}</label>`)}</div></div>` });
  }

  async function loadClock() {
    const el = $('#display-clock');
    if (!el) return;
    try { const t = await T.Api.time(); el.textContent = !t.rtc ? 'no clock chip detected' : (t.timeSet ? t.time : 'not set'); }
    catch (e) { el.textContent = 'unavailable'; }
  }
  async function setClock() {
    try { await T.Api.setTime(); T.Shell.toast('Clock set'); loadClock(); }
    catch (e) { T.Shell.toast('Could not set the clock: ' + e.message, 'danger'); }
  }

  T.definePage({
    id: 'display', title: 'Display & alarms',
    nav: { group: 'more', icon: 'monitor', order: 2 },
    boards: ['display'],
    save: { reboot: false },
    render() {
      const c = T.State.config;
      return html`${UI.pagehead('Display & alarms', 'The screen shows a clock page and a sensor page; the two buttons beside it switch between them. Changes apply as soon as you save.')}
        ${UI.card({ title: 'Screen lines', icon: 'monitor', body: html`
          <p class="hint">Templates: placeholders are replaced with live readings. About 18 characters fit per line. A placeholder with no matching sensor is shown as written — that is how you spot a typo.</p>
          ${UI.field({ id: 'line1', label: 'Line 1', labelHint: 'sensor page and clock footer', value: c.line1, placeholder: '%temp% °C', attrs: 'maxlength="40"' })}
          ${UI.field({ id: 'line2', label: 'Line 2', labelHint: 'sensor page and clock footer', value: c.line2, placeholder: '%humi% %RH', attrs: 'maxlength="40"' })}
          ${UI.field({ id: 'line3', label: 'Line 3', labelHint: 'sensor page only', value: c.line3, placeholder: '%qfe% hPa', attrs: 'maxlength="40"' })}
          <div class="field"><label>Insert a reading <span class="hint">into the line you last edited</span></label><div id="line-chips">${UI.chips('line1')}</div></div>` })}
        <div class="grid-2">
        ${UI.card({ title: 'Clock', icon: 'clock', body: html`
          ${UI.toggle({ id: 'clock_12h', label: '12-hour clock', checked: !!c.clock_12h, hint: '1–12 with am/pm instead of 0–23.' })}
          ${UI.toggle({ id: 'clock_show_ip', label: 'Show the IP address', checked: c.clock_show_ip !== false, hint: 'In tiny type along the edge of the clock page, so the device is always easy to find. “wifi off” when WiFi is off.' })}
          <hr>
          <p><strong>Device clock:</strong> <span id="display-clock" class="muted">…</span></p>
          <button type="button" class="btn btn-sm" id="set-clock" data-nosave>${T.icon('clock')} Set from this browser</button>
          <p class="hint mt">The DS3231 keeps time on its backup battery once set. Alarms and the data log need it set.</p>` })}
        ${UI.card({ title: 'Night mode', icon: 'moon', body: html`
          ${UI.toggle({ id: 'clock_sleep', label: 'Switch the screen off at night', checked: !!c.clock_sleep, hint: 'The panel goes dark inside the window; alarms still ring (and light it while ringing), buttons still work, data is still sent.' })}
          <div class="fields-inline mt">
            ${UI.field({ id: 'clock_sleep_start', label: 'Off from', type: 'time', value: hhmm(c.clock_sleep_start) || '22:00' })}
            ${UI.field({ id: 'clock_sleep_finish', label: 'On again at', type: 'time', value: hhmm(c.clock_sleep_finish) || '07:00' })}
          </div>
          <p class="hint mt mb0">A window across midnight (22:00 → 07:00) works as expected.</p>` })}
        </div>
        <h2>Alarms</h2>
        <p class="hint">Up to three weekday alarms. When one fires the buzzer alternates two tones once a second and the screen shows the message. <strong>Press any screen button to mute.</strong></p>
        <div class="grid-3">${[1, 2, 3].map(n => alarmCard(n, c))}</div>
        ${UI.card({ title: 'Buttons on the device', icon: 'cpu', body: UI.table(['Button', 'Press', 'Does'], [
          [html`<strong>Left / Right</strong>`, 'click', 'Switch between the clock and sensor pages — or mute a ringing alarm.'],
          [html`<strong>Right</strong>`, 'hold 10 s', html`Toggle offline mode: WiFi off, screen and sensors keep running. The LED turns ${T.led('purple')} and the device restarts. The same hold switches WiFi back on.`],
          [html`<strong>MODE</strong> (top)`, 'press after RESET', html`Setup mode with the setup access point; the LED turns ${T.led('blue')}. Holding it during RESET starts firmware-flashing mode instead.`],
          [html`<strong>MODE</strong> (top)`, 'hold 20 s', html`Factory reset: settings, WiFi credentials and the data log are erased. The LED turns ${T.led('red')}.`],
          [html`<strong>RESET</strong>`, 'click', 'Restart.']
        ]) })}`;
    },
    mount(root) {
      loadClock();
      clockTimer = setInterval(loadClock, 30000);
      root.addEventListener('focusin', e => {
        if (/^line[123]$/.test(e.target.id || '')) T.$$('#line-chips [data-insert]', root).forEach(b => b.setAttribute('data-target', e.target.id));
      });
      root.addEventListener('click', e => { if (e.target.closest('#set-clock')) setClock(); });
    },
    unmount() { clearInterval(clockTimer); clockTimer = null; },
    on: {
      sensors() { const cur = T.$('#line-chips [data-insert]'); const target = cur ? cur.getAttribute('data-target') : 'line1'; T.render($('#line-chips'), UI.chips(target)); }
    },
    collect() {
      const out = {
        line1: T.val('line1'), line2: T.val('line2'), line3: T.val('line3'),
        clock_12h: T.checked('clock_12h'), clock_show_ip: T.checked('clock_show_ip'),
        clock_sleep: T.checked('clock_sleep'),
        clock_sleep_start: hhmm(T.val('clock_sleep_start')), clock_sleep_finish: hhmm(T.val('clock_sleep_finish'))
      };
      if (out.clock_sleep && (!out.clock_sleep_start || !out.clock_sleep_finish)) throw T.fail('Night mode needs both times', out.clock_sleep_start ? 'clock_sleep_finish' : 'clock_sleep_start');
      for (let n = 1; n <= 3; n++) {
        const p = 'alarm' + n;
        out[p + 'Active'] = T.checked(p + 'Active');
        out[p + 'Time'] = hhmm(T.val(p + 'Time'));
        out[p + 'Message'] = T.val(p + 'Message');
        const flags = [];
        for (let d = 0; d < 7; d++) { const cb = T.$('input[data-alarm="' + n + '"][data-day="' + d + '"]'); flags.push(cb && cb.checked ? '1' : '0'); }
        out[p + 'Weekdays'] = flags.join(',');
        if (out[p + 'Active'] && !out[p + 'Time']) throw T.fail('Alarm ' + n + ' needs a time', p + 'Time');
        if (out[p + 'Active'] && flags.indexOf('1') < 0) throw T.fail('Alarm ' + n + ' needs at least one day', p + 'Time');
      }
      return out;
    }
  });
})();
