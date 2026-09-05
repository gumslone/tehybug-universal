/* Send data: every destination on one page — TeHyBug Cloud, Home Assistant
 * / MQTT, HTTP GET and HTTP POST — with the firmware's exclusivity rules
 * applied as you switch things on, not discovered after saving. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI, $ = T.$;

  const cloudFallback = () => T.CLOUD_URL + '?bug_key=%key%&t=%temp%&h=%humi%';
  // The firmware clamps intervals to 10 s .. the longest deep sleep the chip
  // can do (about 3.5 h); say so where the number is typed.
  const maxInterval = () => (T.isDisplay() ? 0 : parseInt(T.State.info.deepSleepMax, 10) || 0);
  const intervalHint = () => (maxInterval() ? 'At least 10 s, at most ' + T.fmt.secs(maxInterval()) + ' (the longest sleep the chip can do).' : 'At least 10 s.');
  const clampInterval = v => { const m = maxInterval(); return Math.max(10, m ? Math.min(m, v) : v); };
  // https needs the TLS client, which the 1 MB build for first-generation boards leaves out
  const checkScheme = (url, fieldId) => { if (T.isGeneric() && /^https:/i.test(url)) throw T.fail('First-generation boards have no TLS client — use an http:// address', fieldId); };
  // The cloud always gets metric values (tehybug.com charts °C), whatever the
  // °C/°F switch shows; and until this session has seen readings, a stored
  // cloud URL is kept rather than replaced by the two-value fallback.
  const cloudPreview = () => {
    if (T.Suggest.have()) return T.Suggest.cloudUrl('metric');
    const stored = T.State.config.httpGetURL;
    return T.isCloudUrl(stored) ? stored : cloudFallback();
  };

  function cloudCard(c) {
    const on = !!c.httpGetActive && T.isCloudUrl(c.httpGetURL);
    const key = T.State.info.key || c.key || '';
    return UI.card({ title: 'TeHyBug Cloud', icon: 'cloud', body: html`
      <p class="hint">The easiest way: readings go to your account on <a href="https://tehybug.com" target="_blank" rel="noopener">tehybug.com</a> — charts, history, e-mail and Telegram alerts, nothing to host.</p>
      ${UI.toggle({ id: 'cloudActive', cls: 'big', label: 'Send to TeHyBug Cloud', checked: on })}
      <div id="cloud-fields" ${on ? '' : 'hidden'}>
        <div class="field"><label>Your device key</label><div class="row"><code>${key || '…'}</code>${key ? html`<button type="button" class="btn btn-sm" data-copy="${key}">${T.icon('copy')} Copy</button>` : ''}</div><div class="hint">Add the device to your tehybug.com account with this key.</div></div>
        ${UI.field({ id: 'cloudFreq', label: 'Send every', labelHint: 'seconds', type: 'number', value: on && c.httpGetFrequency ? c.httpGetFrequency : 900, attrs: 'min="60" inputmode="numeric"', hint: '900 s (15 min) suits a battery device; the cloud keeps the history either way. ' + intervalHint() })}
        <div class="hint">The device will request <code id="cloud-url">${cloudPreview()}</code> — always in °C, whatever the °C/°F switch shows.</div>
      </div>` });
  }

  function mqttCard(c) {
    const mode = c.haActive ? 'ha' : (c.mqttActive ? 'custom' : 'off');
    // the 1 MB build for first-generation boards has no Home Assistant support
    const options = [{ value: 'off', label: 'Off' }];
    if (!T.isGeneric()) options.push({ value: 'ha', label: 'Home Assistant', hint: 'The sensors appear in Home Assistant by themselves (MQTT auto-discovery).' });
    options.push({ value: 'custom', label: 'Publish to an MQTT topic', hint: 'Your broker, your topic, your payload.' });
    return UI.card({ title: T.isGeneric() ? 'MQTT' : 'Home Assistant & MQTT', icon: 'home', body: html`
      ${UI.choice({ name: 'mqttMode', value: mode, options })}
      <div id="mqtt-fields" ${mode === 'off' ? 'hidden' : ''}>
        <h3 class="mt">Broker</h3>
        ${UI.field({ id: 'mqttServer', label: 'Server', labelHint: 'IP or hostname', value: c.mqttServer === '0.0.0.0' ? '' : c.mqttServer, placeholder: '192.168.1.10 or homeassistant.local', attrs: 'autocomplete="off" maxlength="63" inputmode="url"' })}
        <div class="fields-inline">
          ${UI.field({ id: 'mqttPort', label: 'Port', type: 'number', value: c.mqttPort || 1883, attrs: 'inputmode="numeric"', hint: 'Usually 1883 — plain MQTT; this firmware does not speak TLS.' })}
          ${UI.field({ id: 'mqttFrequency', label: 'Send every', labelHint: 's', type: 'number', value: c.mqttFrequency || 600, attrs: 'min="10" inputmode="numeric"', hint: intervalHint() })}
        </div>
        <div class="fields-inline">
          ${UI.field({ id: 'mqttUser', label: 'User', value: c.mqttUser, placeholder: 'optional', attrs: 'autocomplete="off"' })}
          ${UI.password({ id: 'mqttPassword', label: 'Password', value: c.mqttPassword, placeholder: 'optional', hint: c.mqttPassword === '********' ? 'A password is stored; it is never shown here. Type to replace it, clear the field to remove it.' : '' })}
        </div>
        <div id="mqtt-ha" ${mode === 'ha' ? '' : 'hidden'}>${UI.note('info', html`In Home Assistant: install the <strong>Mosquitto broker</strong> add-on, create a user for it (Settings → People), enable the <strong>MQTT</strong> integration, and enter that user above with Home Assistant's address as the server. The TeHyBug shows up under Settings → Devices &amp; services → MQTT after its first send.`)}</div>
        <div id="mqtt-custom" ${mode === 'custom' ? '' : 'hidden'}>
          <h3 class="mt">Topic &amp; payload</h3>
          ${UI.field({ id: 'mqttMasterTopic', label: 'Topic', value: c.mqttMasterTopic, placeholder: 'home/sensors/tehybug' })}
          ${UI.field({ id: 'mqttMessage', label: 'Payload template', value: c.mqttMessage, placeholder: '{"temp":"%temp%", "humi":"%humi%"}', after: UI.fill('mqttMessage', 'json'), hint: html`<code>%placeholders%</code> are replaced with readings before publishing; an unknown one is sent as written, which is how you spot a typo.` })}
          ${UI.toggle({ id: 'mqttRetained', label: 'Retained', checked: !!c.mqttRetained, hint: 'The broker keeps the last message for new subscribers.' })}
        </div>
      </div>` });
  }

  function getCard(c) {
    const custom = !!c.httpGetActive && !T.isCloudUrl(c.httpGetURL);
    const url = T.isCloudUrl(c.httpGetURL) ? '' : (c.httpGetURL || '');
    return UI.card({ title: 'HTTP GET — your own server', icon: 'download', body: html`
      ${UI.toggle({ id: 'getActive', label: 'Request a URL with the readings', checked: custom, hint: 'For simple logging services and webhooks: the values ride in the query string.' })}
      <div id="get-fields" ${custom ? '' : 'hidden'}>
        ${UI.field({ id: 'httpGetURL', label: 'URL', type: 'url', value: url, placeholder: 'https://example.com/log?device=%key%&t=%temp%', attrs: 'inputmode="url" autocomplete="off"', after: UI.fill('httpGetURL', 'query', 'Keep the server, rebuild the query from my sensors:') })}
        ${UI.field({ id: 'httpGetFrequency', label: 'Send every', labelHint: 'seconds', type: 'number', value: c.httpGetFrequency || 900, attrs: 'min="10" inputmode="numeric"', hint: intervalHint() })}
      </div>` });
  }

  function postCard(c) {
    return UI.card({ title: 'HTTP POST — JSON to an API', icon: 'upload', body: html`
      ${UI.toggle({ id: 'postActive', label: 'POST a JSON body', checked: !!c.httpPostActive, hint: 'For REST APIs and anything that wants structured data.' })}
      <div id="post-fields" ${c.httpPostActive ? '' : 'hidden'}>
        ${UI.field({ id: 'httpPostURL', label: 'URL', type: 'url', value: c.httpPostURL, placeholder: 'https://example.com/api/readings', attrs: 'inputmode="url" autocomplete="off"' })}
        ${UI.field({ id: 'httpPostJson', label: 'JSON body template', value: c.httpPostJson, placeholder: '{"device":"%key%","temp":"%temp%","humi":"%humi%"}', after: UI.fill('httpPostJson', 'json') })}
        ${UI.field({ id: 'httpPostFrequency', label: 'Send every', labelHint: 'seconds', type: 'number', value: c.httpPostFrequency || 900, attrs: 'min="10" inputmode="numeric"', hint: intervalHint() })}
      </div>` });
  }

  // Optional certificate pin for https targets. The device has no clock to
  // validate a certificate chain against, so the check it can do is "this
  // exact certificate" — which also means a renewed certificate breaks it
  // until the pin is updated.
  function tlsCard(c) {
    if (T.isGeneric()) return '';
    return UI.card({ title: 'HTTPS certificate check', icon: 'key', body: html`
      <p class="hint">Optional. An https:// target is encrypted either way, but by default the device does not verify who it is talking to. Set the fingerprint of your server's certificate and it refuses anything else.</p>
      ${UI.field({ id: 'httpsFingerprint', textarea: true, rows: 2, label: 'SHA-1 fingerprint of the server certificate', labelHint: 'optional', value: c.httpsFingerprint, placeholder: 'AB:CD:EF:… (20 pairs)', attrs: 'autocomplete="off" spellcheck="false"', hint: html`Read it in your browser's certificate viewer, or with <code>openssl s_client -connect host:443 -servername host &lt;/dev/null | openssl x509 -noout -fingerprint -sha1</code>. One bare fingerprint applies to every HTTPS target; for several servers write one entry per line as <code>host.example.com AB:CD:…</code> (a host without an entry is sent to unverified). Certificates get renewed (Let's Encrypt about every two months): after a renewal the sends fail until you update it — the dashboard log then shows the TLS reason. Leave empty for no check. MQTT has no TLS at all.` })}
      <div class="row"><button type="button" class="btn btn-sm" id="tls-test" data-nosave>${T.icon('check')} Test the HTTPS targets now</button><span class="hint">The device connects to each https:// address on this page with the fingerprint above, before anything is saved.</span></div>
      <div id="tls-test-result" class="mt"></div>` });
  }

  // the firmware's rule (pinForHost in data_service.h): a host's own entry
  // wins over a bare one; no entry means no pin
  function pinFor(entries, host) {
    let bare = '';
    for (const e of entries) {
      const sp = e.lastIndexOf(' ');
      if (sp < 0) { if (!bare) bare = e; continue; }
      if (e.slice(0, sp).toLowerCase() === host.toLowerCase()) return e.slice(sp + 1);
    }
    return bare;
  }
  async function testTls() {
    const box = $('#tls-test-result'), btn = $('#tls-test');
    let entries;
    try { entries = T.val('httpsFingerprint').split(/[\n,;]+/).map(s => s.trim()).filter(Boolean).map(normalisePin); }
    catch (e) { T.Shell.toast(e.message, 'danger', 5000); return; }
    const urls = [];
    if (T.checked('getActive') && /^https:/i.test(T.val('httpGetURL'))) urls.push(T.val('httpGetURL').trim());
    if (T.checked('postActive') && /^https:/i.test(T.val('httpPostURL'))) urls.push(T.val('httpPostURL').trim());
    // hosts named in the pins but not among the targets are worth a look too
    const bareHost = u => T.hostOf(u).replace(/:\d+$/, '').toLowerCase();
    entries.forEach(e => { const sp = e.lastIndexOf(' '); if (sp > 0) { const h = e.slice(0, sp); if (!urls.some(u => bareHost(u) === h)) urls.push('https://' + h + '/'); } });
    if (!urls.length) { T.render(box, UI.note('info', 'Nothing to test: switch on an https:// HTTP GET or POST target above (or name a host in the fingerprint field) first.')); return; }
    btn.disabled = true;
    T.render(box, html`<div class="hint">Testing ${urls.length} target${urls.length === 1 ? '' : 's'}… each handshake takes a few seconds.</div>`);
    const rows = [];
    for (const url of urls) {
      const host = T.hostOf(url);
      const pin = pinFor(entries, host.replace(/:\d+$/, ''));
      try {
        const r = await T.Api.testTls(url, pin);
        if (r.ok && r.verified) rows.push(['ok', host, 'certificate matches the fingerprint']);
        else if (r.ok) rows.push(['info', host, 'reachable — no fingerprint for this host, so the connection would be unverified']);
        else if (r.code === 62) rows.push(['danger', host, 'the certificate does not match the fingerprint (wrong entry, or the server renewed its certificate)']);
        else rows.push(['danger', host, (r.error || 'failed') + (r.code ? ' (TLS error ' + r.code + ')' : '')]);
      } catch (e) { rows.push(['danger', host, 'the device did not answer: ' + e.message]); }
    }
    T.render(box, html`${rows.map(r => UI.note(r[0], html`<strong>${r[1]}</strong>: ${r[2]}`))}`);
    btn.disabled = false;
  }

  // "AB:CD:…" or "host AB:CD:…" per line/comma; returns the canonical entry
  // or throws with the offending text
  function normalisePin(entry) {
    // an optional host, then 20 hex pairs with any separator (or none)
    const m = /^\s*(?:([A-Za-z0-9.-]+)\s+)?((?:[0-9a-fA-F]{2}[\s:.-]*){19}[0-9a-fA-F]{2})\s*$/.exec(entry);
    if (!m) throw T.fail('Check the fingerprint entry "' + entry.trim() + '": a SHA-1 fingerprint is 40 hex digits (20 pairs), optionally preceded by a host name', 'httpsFingerprint');
    const host = (m[1] || '').toLowerCase();
    const hex = m[2].replace(/[^0-9a-fA-F]/g, '').toUpperCase();
    return (host ? host + ' ' : '') + hex.match(/../g).join(':');
  }

  // The rules the firmware lives by, applied as switches flip:
  //  - Home Assistant owns the MQTT client and its availability topic, so
  //    it runs alone (the firmware serves one way at a time with HA).
  //  - TeHyBug Cloud is HTTP GET with a fixed server: one or the other.
  //  - Offline mode never brings WiFi up, so any network destination
  //    switches it off on save.
  function applyRules(changedId) {
    const notes = [];
    let mode = T.radio('mqttMode');
    let cloud = T.checked('cloudActive'), get = T.checked('getActive'), post = T.checked('postActive');
    if (changedId === 'mqttMode' && mode === 'ha' && (cloud || get || post)) {
      T.setChecked('cloudActive', false); T.setChecked('getActive', false); T.setChecked('postActive', false);
      cloud = get = post = false;
      notes.push('Home Assistant runs on its own — TeHyBug Cloud and HTTP sending were switched off.');
    }
    if ((changedId === 'cloudActive' || changedId === 'getActive' || changedId === 'postActive') && mode === 'ha' && (cloud || get || post)) {
      T.setRadio('mqttMode', 'off'); mode = 'off';
      notes.push('Home Assistant was switched off — it cannot run alongside another destination.');
    }
    if (changedId === 'cloudActive' && cloud && get) { T.setChecked('getActive', false); get = false; notes.push('Custom HTTP GET was switched off — TeHyBug Cloud uses the same HTTP GET slot.'); }
    if (changedId === 'getActive' && get && cloud) { T.setChecked('cloudActive', false); cloud = false; notes.push('TeHyBug Cloud was switched off — it is an HTTP GET to tehybug.com, and the device does one GET target.'); }
    const anyNet = cloud || get || post || mode !== 'off';
    if (anyNet && T.State.config.offlineModeActive) notes.push('Offline mode is currently on; saving switches it off so the device connects to WiFi.');
    T.show('cloud-fields', cloud);
    T.show('get-fields', get);
    T.show('post-fields', post);
    T.show('mqtt-fields', mode !== 'off');
    T.show('mqtt-ha', mode === 'ha');
    T.show('mqtt-custom', mode === 'custom');
    const box = $('#rule-notes');
    if (box) { T.render(box, notes.length ? UI.note('info', html`${notes.map(n => html`<div>${n}</div>`)}`) : ''); }
    const preview = $('#cloud-url');
    if (preview) preview.textContent = cloudPreview();
  }

  T.definePage({
    id: 'senddata', title: 'Send data',
    nav: { group: 'setup', icon: 'send', order: 2 },
    save: { reboot: true },
    offersGoLive: true,
    render() {
      const c = T.State.config;
      return html`${UI.pagehead('Send data', 'Where the readings go. Everything below is stored on the device; sending starts when it goes live.')}
        <div id="rule-notes"></div>
        ${cloudCard(c)}
        ${mqttCard(c)}
        ${getCard(c)}
        ${postCard(c)}
        ${tlsCard(c)}
        ${UI.card({ title: 'Placeholders this device provides', icon: 'list', actions: UI.unitsSeg(), body: html`<div id="ph-list">${UI.placeholderList()}</div><p class="hint mt">Use them in URLs and payloads (the MQTT topic is sent as written); <code>%key%</code> is the device key. The °C/°F switch also decides what the “fill from my sensors” links produce.</p>` })}`;
    },
    mount(root) {
      root.addEventListener('change', e => {
        const id = e.target.name === 'mqttMode' ? 'mqttMode' : e.target.id;
        if (['mqttMode', 'cloudActive', 'getActive', 'postActive'].indexOf(id) >= 0) applyRules(id);
      });
      root.addEventListener('click', e => { if (e.target.closest('#tls-test')) testTls(); });
    },
    on: {
      sensors() { T.render($('#ph-list'), UI.placeholderList()); const p = $('#cloud-url'); if (p) p.textContent = cloudPreview(); },
      units() { T.render($('#ph-list'), UI.placeholderList()); },
      info() { if (!T.Shell.dirty) T.Shell.rerender(); }
    },
    collect() {
      const cloud = T.checked('cloudActive'), get = T.checked('getActive'), post = T.checked('postActive');
      const mode = T.radio('mqttMode');
      const ha = mode === 'ha', mqtt = mode === 'custom';
      const out = {};
      if (cloud) {
        out.httpGetActive = true;
        out.httpGetURL = cloudPreview();
        out.httpGetFrequency = clampInterval(T.int('cloudFreq', 900));
      } else {
        out.httpGetActive = get;
        out.httpGetURL = T.val('httpGetURL').trim();
        out.httpGetFrequency = clampInterval(T.int('httpGetFrequency', 900));
        if (get && !T.isUrl(out.httpGetURL)) throw T.fail('Enter the full HTTP GET URL, starting with http:// or https://', 'httpGetURL');
        if (get) checkScheme(out.httpGetURL, 'httpGetURL');
      }
      out.httpPostActive = post;
      out.httpPostURL = T.val('httpPostURL').trim();
      out.httpPostJson = T.val('httpPostJson').trim();
      out.httpPostFrequency = clampInterval(T.int('httpPostFrequency', 900));
      if (post && !T.isUrl(out.httpPostURL)) throw T.fail('Enter the full HTTP POST URL, starting with http:// or https://', 'httpPostURL');
      if (post) checkScheme(out.httpPostURL, 'httpPostURL');
      out.mqttServer = T.val('mqttServer').trim();
      out.mqttPort = T.int('mqttPort', 1883);
      out.mqttUser = T.val('mqttUser');
      out.mqttPassword = T.val('mqttPassword');
      out.mqttFrequency = clampInterval(T.int('mqttFrequency', 600));
      out.mqttMasterTopic = T.val('mqttMasterTopic').trim();
      out.mqttMessage = T.val('mqttMessage').trim();
      out.mqttRetained = T.checked('mqttRetained');
      out.haActive = ha;
      out.mqttActive = mqtt;
      if ((ha || mqtt) && !out.mqttServer) throw T.fail('Enter the MQTT broker address', 'mqttServer');
      if (mqtt && !out.mqttMasterTopic) throw T.fail('Enter the MQTT topic to publish to', 'mqttMasterTopic');
      if (!out.mqttServer) out.mqttServer = '0.0.0.0'; // the firmware's "unset"
      if (ha) { out.httpGetActive = false; out.httpPostActive = false; }
      if (out.httpGetActive || out.httpPostActive || ha || mqtt) out.offlineModeActive = false;
      if (!T.isGeneric()) {
        // one entry per line (or comma); stored canonical, newline-separated
        out.httpsFingerprint = T.val('httpsFingerprint').split(/[\n,;]+/).map(s => s.trim()).filter(Boolean).map(normalisePin).join('\n');
      }
      return out;
    }
  });
})();
