/* Scenarios: three "if a reading crosses a value, do this" rules. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI, $ = T.$;
  const COUNT = 3;
  const TYPES = [
    { value: 'get', label: 'Request a URL (HTTP GET)' },
    { value: 'post', label: 'POST JSON to a URL' },
    { value: 'io13_1', label: 'Switch IO_13 on (HIGH, 3.3 V)' },
    { value: 'io13_0', label: 'Switch IO_13 off (LOW)' }
  ];
  const CONDITIONS = [{ value: 'gt', label: 'is above' }, { value: 'lt', label: 'is below' }, { value: 'eq', label: 'equals' }];

  function dataOptions() {
    // whatever this device measures, with the two classics always present
    const keys = T.Readings.known().filter(k => !/^cs2?$/.test(k));
    ['temp', 'humi'].forEach(k => { if (keys.indexOf(k) < 0) keys.push(k); });
    return keys.map(k => ({ value: k, label: T.Readings.name(k) + (T.Readings.unit(k) ? ' (' + T.Readings.unit(k) + ')' : '') }));
  }

  function scenarioCard(n, c) {
    const p = 'sc' + n + '_';
    const type = c[p + 'type'] || 'get';
    const isHttp = type === 'get' || type === 'post';
    return UI.card({ title: 'Scenario ' + n, icon: 'layers', body: html`
      ${UI.toggle({ id: p + 'active', label: 'Enabled', checked: !!c[p + 'active'] })}
      <div class="fields-inline mt">
        ${UI.select({ id: p + 'data', label: 'When', options: dataOptions(), value: c[p + 'data'] || 'temp' })}
        ${UI.select({ id: p + 'condition', label: '', options: CONDITIONS, value: c[p + 'condition'] || 'gt' })}
      </div>
      ${UI.field({ id: p + 'value', label: 'Value', type: 'number', value: c[p + 'value'] == null ? '' : c[p + 'value'], attrs: 'step="any" inputmode="decimal"', placeholder: '25' })}
      ${UI.select({ id: p + 'type', label: 'Then', options: TYPES, value: type })}
      <div data-http="${n}" ${isHttp ? '' : 'hidden'}>
        ${UI.field({ id: p + 'url', label: 'URL', type: 'url', value: c[p + 'url'], placeholder: 'https://maker.ifttt.com/trigger/high_temp/with/key/…', attrs: 'inputmode="url" autocomplete="off"' })}
        <div data-post="${n}" ${type === 'post' ? '' : 'hidden'}>
          ${UI.field({ id: p + 'message', label: 'JSON body', value: c[p + 'message'], placeholder: '{"alert":"high_temp","value":"%temp%"}', after: UI.fill(p + 'message', 'json') })}
        </div>
      </div>` });
  }

  T.definePage({
    id: 'scenarios', title: 'Scenarios',
    nav: { group: 'more', icon: 'layers', order: 0 },
    save: { reboot: true, label: 'Save scenarios' },
    render() {
      const c = T.State.config;
      return html`${UI.pagehead('Scenarios', 'If a reading crosses a value, request a URL or switch a pin — checked on every send, while the device is live.')}
        <div class="grid-3">${[1, 2, 3].map(n => scenarioCard(n, c))}</div>
        ${UI.card({ title: 'Good to know', icon: 'info', body: html`
          <ul class="small mb0">
            <li><strong>No debouncing:</strong> while the condition stays true, the action fires on every send. Pair two scenarios (above → on, below → off) for a thermostat-style switch.</li>
            <li><strong>IO_13</strong> can source about 12 mA at 3.3 V — enough for an LED or a relay module's input, not a load.</li>
            <li><strong>Placeholders</strong> such as <code>%temp%</code> work in the URL and the JSON body.</li>
            <li>HTTP actions need the network; pin actions also work in offline mode.</li>
          </ul>` })}`;
    },
    mount(root) {
      root.addEventListener('change', e => {
        const m = /^sc(\d)_type$/.exec(e.target.id || '');
        if (!m) return;
        const n = m[1], v = e.target.value;
        T.$('[data-http="' + n + '"]', root).hidden = !(v === 'get' || v === 'post');
        T.$('[data-post="' + n + '"]', root).hidden = v !== 'post';
      });
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
