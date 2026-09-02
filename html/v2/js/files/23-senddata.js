/* Send data: every destination on one page — TeHyBug Cloud, Home Assistant
 * / MQTT, HTTP GET and HTTP POST — with the firmware's exclusivity rules
 * applied as you switch things on, not discovered after saving. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI, $ = T.$;

  const cloudFallback = () => T.CLOUD_URL + '?bug_key=%key%&t=%temp%&h=%humi%';
  const cloudPreview = () => T.Suggest.have() ? T.Suggest.cloudUrl(T.units()) : cloudFallback();

  function cloudCard(c) {
    const on = !!c.httpGetActive && T.isCloudUrl(c.httpGetURL);
    const key = T.State.info.key || c.key || '';
    return UI.card({ title: 'TeHyBug Cloud', icon: 'cloud', body: html`
      <p class="hint">The easiest way: readings go to your account on <a href="https://tehybug.com" target="_blank" rel="noopener">tehybug.com</a> — charts, history, e-mail and Telegram alerts, nothing to host.</p>
      ${UI.toggle({ id: 'cloudActive', cls: 'big', label: 'Send to TeHyBug Cloud', checked: on })}
      <div id="cloud-fields" ${on ? '' : 'hidden'}>
        <div class="field"><label>Your device key</label><div class="row"><code>${key || '…'}</code>${key ? html`<button type="button" class="btn btn-sm" data-copy="${key}">${T.icon('copy')} Copy</button>` : ''}</div><div class="hint">Add the device to your tehybug.com account with this key.</div></div>
        ${UI.field({ id: 'cloudFreq', label: 'Send every', labelHint: 'seconds', type: 'number', value: on && c.httpGetFrequency ? c.httpGetFrequency : 900, attrs: 'min="60" inputmode="numeric"', hint: '900 s (15 min) suits a battery device; the cloud keeps the history either way.' })}
        <div class="hint">The device will request <code id="cloud-url">${cloudPreview()}</code></div>
      </div>` });
  }

  function mqttCard(c) {
    const mode = c.haActive ? 'ha' : (c.mqttActive ? 'custom' : 'off');
    return UI.card({ title: 'Home Assistant & MQTT', icon: 'home', body: html`
      ${UI.choice({ name: 'mqttMode', value: mode, options: [
        { value: 'off', label: 'Off' },
        { value: 'ha', label: 'Home Assistant', hint: 'The sensors appear in Home Assistant by themselves (MQTT auto-discovery).' },
        { value: 'custom', label: 'Publish to an MQTT topic', hint: 'Your broker, your topic, your payload.' }
      ] })}
      <div id="mqtt-fields" ${mode === 'off' ? 'hidden' : ''}>
        <h3 class="mt">Broker</h3>
        ${UI.field({ id: 'mqttServer', label: 'Server', labelHint: 'IP or hostname', value: c.mqttServer === '0.0.0.0' ? '' : c.mqttServer, placeholder: '192.168.1.10 or homeassistant.local', attrs: 'autocomplete="off" maxlength="63" inputmode="url"' })}
        <div class="fields-inline">
          ${UI.field({ id: 'mqttPort', label: 'Port', type: 'number', value: c.mqttPort || 1883, attrs: 'inputmode="numeric"', hint: '1883 plain, 8883 TLS' })}
          ${UI.field({ id: 'mqttFrequency', label: 'Send every', labelHint: 's', type: 'number', value: c.mqttFrequency || 600, attrs: 'min="10" inputmode="numeric"' })}
        </div>
        <div class="fields-inline">
          ${UI.field({ id: 'mqttUser', label: 'User', value: c.mqttUser, placeholder: 'optional', attrs: 'autocomplete="off"' })}
          ${UI.password({ id: 'mqttPassword', label: 'Password', value: c.mqttPassword, placeholder: 'optional' })}
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
        ${UI.field({ id: 'httpGetFrequency', label: 'Send every', labelHint: 'seconds', type: 'number', value: c.httpGetFrequency || 900, attrs: 'min="10" inputmode="numeric"' })}
      </div>` });
  }

  function postCard(c) {
    return UI.card({ title: 'HTTP POST — JSON to an API', icon: 'upload', body: html`
      ${UI.toggle({ id: 'postActive', label: 'POST a JSON body', checked: !!c.httpPostActive, hint: 'For REST APIs and anything that wants structured data.' })}
      <div id="post-fields" ${c.httpPostActive ? '' : 'hidden'}>
        ${UI.field({ id: 'httpPostURL', label: 'URL', type: 'url', value: c.httpPostURL, placeholder: 'https://example.com/api/readings', attrs: 'inputmode="url" autocomplete="off"' })}
        ${UI.field({ id: 'httpPostJson', label: 'JSON body template', value: c.httpPostJson, placeholder: '{"device":"%key%","temp":"%temp%","humi":"%humi%"}', after: UI.fill('httpPostJson', 'json') })}
        ${UI.field({ id: 'httpPostFrequency', label: 'Send every', labelHint: 'seconds', type: 'number', value: c.httpPostFrequency || 900, attrs: 'min="10" inputmode="numeric"' })}
      </div>` });
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
        ${UI.card({ title: 'Placeholders this device provides', icon: 'list', actions: UI.unitsSeg(), body: html`<div id="ph-list">${UI.placeholderList()}</div><p class="hint mt">Use them in URLs, payloads and topics; <code>%key%</code> is the device key. The °C/°F switch also decides what the “fill from my sensors” links produce.</p>` })}`;
    },
    mount(root) {
      root.addEventListener('change', e => {
        const id = e.target.name === 'mqttMode' ? 'mqttMode' : e.target.id;
        if (['mqttMode', 'cloudActive', 'getActive', 'postActive'].indexOf(id) >= 0) applyRules(id);
      });
    },
    on: {
      sensors() { T.render($('#ph-list'), UI.placeholderList()); const p = $('#cloud-url'); if (p) p.textContent = cloudPreview(); },
      units() { T.render($('#ph-list'), UI.placeholderList()); const p = $('#cloud-url'); if (p) p.textContent = cloudPreview(); },
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
        out.httpGetFrequency = Math.max(10, T.int('cloudFreq', 900));
      } else {
        out.httpGetActive = get;
        out.httpGetURL = T.val('httpGetURL').trim();
        out.httpGetFrequency = Math.max(10, T.int('httpGetFrequency', 900));
        if (get && !T.isUrl(out.httpGetURL)) throw T.fail('Enter the full HTTP GET URL, starting with http:// or https://', 'httpGetURL');
      }
      out.httpPostActive = post;
      out.httpPostURL = T.val('httpPostURL').trim();
      out.httpPostJson = T.val('httpPostJson').trim();
      out.httpPostFrequency = Math.max(10, T.int('httpPostFrequency', 900));
      if (post && !T.isUrl(out.httpPostURL)) throw T.fail('Enter the full HTTP POST URL, starting with http:// or https://', 'httpPostURL');
      out.mqttServer = T.val('mqttServer').trim();
      out.mqttPort = T.int('mqttPort', 1883);
      out.mqttUser = T.val('mqttUser');
      out.mqttPassword = T.val('mqttPassword');
      out.mqttFrequency = Math.max(10, T.int('mqttFrequency', 600));
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
      return out;
    }
  });
})();
