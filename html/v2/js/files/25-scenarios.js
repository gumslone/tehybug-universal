/* Scenarios: three "if a reading crosses a value, do this" rules. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI, $ = T.$;
  const COUNT = 3;
  const TYPES = [
    { value: 'get', label: 'request a URL (HTTP GET)', short: 'request a URL' },
    { value: 'post', label: 'POST JSON to a URL', short: 'POST JSON' },
    { value: 'io13_1', label: 'switch IO_13 on (HIGH, 3.3 V)', short: 'switch IO_13 on' },
    { value: 'io13_0', label: 'switch IO_13 off (LOW)', short: 'switch IO_13 off' }
  ];
  const CONDITIONS = [{ value: 'gt', label: 'is above' }, { value: 'lt', label: 'is below' }, { value: 'eq', label: 'equals' }];

  // whatever this device measures, the two classics, and whatever the scenario
  // already refers to — a stored key must never be swapped for the first
  // option just because its sensor has not reported yet
  function dataOptions(stored) {
    const keys = T.Readings.known().filter(k => !/^cs2?$/.test(k));
    ['temp', 'humi'].forEach(k => { if (keys.indexOf(k) < 0) keys.push(k); });
    if (stored && keys.indexOf(stored) < 0) keys.push(stored);
    return keys.map(k => ({ value: k, label: T.Readings.name(k) }));
  }
  // "Temperature is above 30 °C → request a URL"
  function summaryText(v) {
    const cond = (CONDITIONS.find(x => x.value === v.condition) || CONDITIONS[0]).label;
    const t = TYPES.find(x => x.value === v.type) || TYPES[0];
    const unit = T.Readings.unit(v.data);
    return T.Readings.name(v.data) + ' ' + cond + ' ' + (v.value === '' || v.value == null ? '…' : v.value) + (unit ? ' ' + unit : '') + ' → ' + t.short;
  }
  const nowText = k => { const v = T.Readings.value(k); const u = T.Readings.unit(k); return v ? 'now ' + v + (u ? ' ' + u : '') : ''; };
  function refreshLive(n) {
    const p = 'sc' + n + '_';
    const k = T.val(p + 'data') || 'temp';
    const unit = document.querySelector('[data-unit="' + n + '"]'), now = document.querySelector('[data-now="' + n + '"]'), sum = document.querySelector('[data-summary="' + n + '"]');
    if (unit) unit.textContent = T.Readings.unit(k);
    if (now) now.textContent = nowText(k);
    if (sum) sum.textContent = summaryText({ data: k, condition: T.val(p + 'condition'), value: T.val(p + 'value'), type: T.val(p + 'type') });
  }
  const opts = (list, value) => html`${list.map(o => html`<option value="${o.value}" ${String(o.value) === String(value) ? 'selected' : ''}>${o.label}</option>`)}`;
  // readings arrived after the page was drawn: grow the lists in place
  function refreshDataOptions() {
    for (let n = 1; n <= COUNT; n++) {
      const sel = document.getElementById('sc' + n + '_data');
      if (!sel || document.activeElement === sel) continue;
      const current = sel.value;
      const opts = dataOptions(current);
      if (opts.length === sel.options.length) continue;
      T.render(sel, html`${opts.map(o => html`<option value="${o.value}" ${o.value === current ? 'selected' : ''}>${o.label}</option>`)}`);
    }
  }

  function scenarioCard(n, c) {
    const p = 'sc' + n + '_';
    const type = c[p + 'type'] || 'get';
    const isHttp = type === 'get' || type === 'post';
    const active = !!c[p + 'active'];
    const k = c[p + 'data'] || 'temp';
    const value = c[p + 'value'] == null ? '' : c[p + 'value'];
    return UI.card({ title: 'Scenario ' + n, icon: 'layers', body: html`
      ${UI.toggle({ id: p + 'active', label: 'Enabled', checked: active })}
      <div class="hint mt" data-summary="${n}" ${active ? 'hidden' : ''}>${summaryText({ data: k, condition: c[p + 'condition'] || 'gt', value, type })}</div>
      <div data-body="${n}" ${active ? '' : 'hidden'}>
        <div class="sentence mt">
          <span class="word">When</span>
          <span class="field field-inline"><select id="${p}data" aria-label="Reading">${opts(dataOptions(c[p + 'data']), k)}</select></span>
          <span class="field field-inline"><select id="${p}condition" aria-label="Condition">${opts(CONDITIONS, c[p + 'condition'] || 'gt')}</select></span>
          <span class="field field-inline value" data-field="${p}value"><input id="${p}value" type="number" step="any" inputmode="decimal" placeholder="25" value="${value}" aria-label="Value"><span class="unit" data-unit="${n}">${T.Readings.unit(k)}</span></span>
          <span class="hint now" data-now="${n}">${nowText(k)}</span>
        </div>
        <div class="sentence">
          <span class="word">then</span>
          <span class="field field-inline"><select id="${p}type" aria-label="Action">${opts(TYPES, type)}</select></span>
        </div>
      <div data-http="${n}" ${isHttp ? '' : 'hidden'}>
        ${UI.field({ id: p + 'url', label: 'URL', type: 'url', value: c[p + 'url'], placeholder: 'https://maker.ifttt.com/trigger/high_temp/with/key/…', attrs: 'inputmode="url" autocomplete="off"' })}
        <div data-post="${n}" ${type === 'post' ? '' : 'hidden'}>
          ${UI.field({ id: p + 'message', label: 'JSON body', value: c[p + 'message'], placeholder: '{"alert":"high_temp","value":"%temp%"}', after: UI.fill(p + 'message', 'json') })}
        </div>
      </div>
      </div>` });
  }

  T.definePage({
    id: 'scenarios', title: 'Scenarios',
    nav: { group: 'more', icon: 'layers', order: 0 },
    // the firmware reads the rules on every send, so a save applies at once
    save: { reboot: false, label: 'Save scenarios' },
    render() {
      const c = T.State.config;
      return html`${UI.pagehead('Scenarios', 'If a reading crosses a value, request a URL or switch a pin — checked on every send, while the device is live.')}
        <div class="grid-3">${[1, 2, 3].map(n => scenarioCard(n, c))}</div>
        ${UI.card({ title: 'Good to know', icon: 'info', body: html`
          <ul class="small mb0">
            <li><strong>No debouncing:</strong> while the condition stays true, the action fires on every send. Pair two scenarios (above → on, below → off) for a thermostat-style switch.</li>
            <li><strong>IO_13</strong> can source about 12 mA at 3.3 V — enough for an LED or a relay module's input, not a load.</li>
            <li><strong>Placeholders</strong> such as <code>%temp%</code> work in the URL and the JSON body.</li>
            <li>Scenarios are checked while the device is live and sending. HTTP actions need the network; on the Display Weatherstation the pin actions keep working in offline mode too.</li>
          </ul>` })}`;
    },
    on: { sensors() { refreshDataOptions(); for (let n = 1; n <= COUNT; n++) refreshLive(n); } },
    mount(root) {
      root.addEventListener('change', e => {
        const m = /^sc(\d)_(\w+)$/.exec(e.target.id || '');
        if (!m) return;
        const n = m[1], v = e.target.value;
        if (m[2] === 'type') {
          T.$('[data-http="' + n + '"]', root).hidden = !(v === 'get' || v === 'post');
          T.$('[data-post="' + n + '"]', root).hidden = v !== 'post';
        }
        if (m[2] === 'active') {
          T.$('[data-body="' + n + '"]', root).hidden = !e.target.checked;
          T.$('[data-summary="' + n + '"]', root).hidden = e.target.checked;
        }
        refreshLive(n);
      });
      root.addEventListener('input', e => { const m = /^sc(\d)_value$/.exec(e.target.id || ''); if (m) refreshLive(m[1]); });
    },
    collect() {
      const out = {};
      for (let n = 1; n <= COUNT; n++) {
        const p = 'sc' + n + '_';
        const type = T.val(p + 'type');
        const active = T.checked(p + 'active');
        out[p + 'active'] = active;
        out[p + 'type'] = type;
        out[p + 'data'] = T.val(p + 'data');
        out[p + 'condition'] = T.val(p + 'condition');
        out[p + 'value'] = T.num(p + 'value', 0);
        out[p + 'url'] = T.val(p + 'url').trim();
        out[p + 'message'] = T.val(p + 'message').trim();
        if (active && T.val(p + 'value').trim() === '') throw T.fail('Scenario ' + n + ' needs a value to compare with', p + 'value');
        if (active && (type === 'get' || type === 'post') && !T.isUrl(out[p + 'url'])) throw T.fail('Scenario ' + n + ' needs a full URL starting with http:// or https://', p + 'url');
      }
      return out;
    }
  });
})();
