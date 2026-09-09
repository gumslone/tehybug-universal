#!/usr/bin/env node
/* Unit tests for the web UI's pure logic (html/v2/js/files/00-core.js):
 * the reading catalogue, payload suggestions, destinations, formatting and
 * the page registry. Runs in Node with a stub window — no DOM needed. */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ctx = { console };
ctx.window = ctx;
ctx.location = { hostname: 'device.local', hash: '' };
ctx.document = { addEventListener() {}, querySelector() { return null; } };
ctx.localStorage = { getItem() { return null; }, setItem() {} };
ctx.URL = URL;
vm.createContext(ctx);
vm.runInContext(fs.readFileSync(path.join(__dirname, '..', '..', 'html', 'v2', 'js', 'files', '00-core.js'), 'utf8'), ctx, { filename: '00-core.js' });
const T = ctx.TeHyBug;

let failed = 0, passed = 0;
function check(name, cond, detail) {
  if (cond) { passed++; return; }
  failed++;
  console.log('FAIL ' + name + (detail !== undefined ? ': ' + JSON.stringify(detail) : ''));
}
const eq = (name, a, b) => check(name, JSON.stringify(a) === JSON.stringify(b), { got: a, want: b });

// --- comfort state: DHTesp's bit field, not a sequence --------------------
T.State.seen = {}; T.State.sensors = {};
[['0', 'Comfortable'], ['1', 'Too hot'], ['2', 'Too cold'], ['4', 'Too dry'], ['8', 'Too humid'], ['9', 'Hot and humid'], ['5', 'Hot and dry'], ['10', 'Cold and humid'], ['6', 'Cold and dry'], ['3', '3']]
  .forEach(([v, label]) => { T.State.sensors.cs = v; eq('comfort ' + v, T.Readings.value('cs'), label); });

// --- imperial siblings follow the firmware's naming --------------------------
eq('impKey temp', T.Readings.impKey('temp'), 'temp_imp');
eq('impKey temp2', T.Readings.impKey('temp2'), 'temp2_imp');
eq('impKey hi2', T.Readings.impKey('hi2'), 'hi_imp2');
eq('impKey dew2', T.Readings.impKey('dew2'), 'dew_imp2');
eq('impKey humi', T.Readings.impKey('humi'), null);
eq('baseKey hi_imp2', T.Readings.baseKey('hi_imp2'), 'hi2');
eq('baseKey temp2_imp', T.Readings.baseKey('temp2_imp'), 'temp2');
eq('unit imperial', T.Readings.unit('temp_imp'), '°F');
eq('unit metric', T.Readings.unit('temp'), '°C');

// --- suggestions from what the device really has -----------------------------
T.State.seen = {}; T.State.sensors = {};
T.applySensors({ key: 'k', temp: '22.5', temp_imp: '72.5', humi: '40.0', qfe: '1013.2', cs: '0', lux: '120', key2: 'x' });
eq('known order', T.Readings.known(), ['temp', 'humi', 'qfe', 'cs', 'lux', 'key2']);
eq('query metric', T.Suggest.query('metric'), 'bug_key=%key%&t=%temp%&h=%humi%&p=%qfe%&l=%lux%');
eq('query imperial swaps only what exists', T.Suggest.query('imperial'), 'bug_key=%key%&t=%temp_imp%&h=%humi%&p=%qfe%&l=%lux%');
eq('json metric', T.Suggest.json('metric'), '{"temp":"%temp%", "humi":"%humi%", "qfe":"%qfe%", "lux":"%lux%"}');
eq('log template', T.Suggest.logTemplate('metric'), '%temp% %humi% %qfe% %lux%');
eq('cloud url', T.Suggest.cloudUrl('metric'), 'http://tehybug.com/track/?bug_key=%key%&t=%temp%&h=%humi%&p=%qfe%&l=%lux%');
check('key is not a reading', !T.State.seen.key);
eq('placeholder chips skip comfort', T.Suggest.keys('metric').indexOf('cs'), -1);

// --- destinations mirror the firmware's precedence ---------------------------
T.State.config = { httpGetActive: true, httpGetURL: 'http://tehybug.com/track/?bug_key=%key%', httpGetFrequency: 900, httpPostActive: true, httpPostURL: 'https://api.example.com/x', httpPostFrequency: 60, haActive: false, mqttActive: true, mqttServer: 'broker', mqttMasterTopic: 'home/t', mqttFrequency: 3600, offlineModeActive: false, eepromLogActive: true, eepromLogFrequency: 300 };
eq('destinations', T.destinations().map(d => d.id), ['cloud', 'post', 'mqtt', 'eeprom']);
eq('cloud detail', T.destinations()[0].detail, 'every 15 min');
eq('post host', T.destinations()[1].detail, 'api.example.com · every 1 min');
T.State.config.offlineModeActive = true;
eq('offline replaces eeprom', T.destinations().map(d => d.id), ['cloud', 'post', 'mqtt', 'offline']);
T.State.config.httpGetURL = 'https://example.org/log?t=%temp%';
eq('custom get', T.destinations()[0].id, 'get');

// --- formatting --------------------------------------------------------------
eq('secs 900', T.fmt.secs(900), '15 min');
eq('secs 12884', T.fmt.secs(12884), '3 h 35 min');
eq('secs 45', T.fmt.secs(45), '45 s');
eq('secs 3600', T.fmt.secs(3600), '1 h');
eq('bytes', T.fmt.bytes(1449984), '1.38 MB');
eq('duration', T.fmt.duration(90061), '1 d 1 h');
eq('hostOf', T.hostOf('https://user@example.com:8443/a/b'), 'example.com:8443');
check('isCloudUrl', T.isCloudUrl('http://tehybug.com/track/?bug_key=1') && !T.isCloudUrl('https://example.com/track'));

// --- page registry is prototype-safe -----------------------------------------
T.definePage({ id: 'dashboard', title: 'D', render() { return T.html`x`; } });
eq('page lookup', T.Pages.get('dashboard').title, 'D');
eq('constructor is not a page', T.Pages.get('constructor'), undefined);
eq('__proto__ is not a page', T.Pages.get('__proto__'), undefined);

// --- templating escapes device strings ---------------------------------------
eq('escape', T.html`<b>${'<script>x</script>'}</b>`.__html, '<b>&lt;script&gt;x&lt;/script&gt;</b>');
eq('raw passes', T.html`${T.raw('<i>ok</i>')}`.__html, '<i>ok</i>');
eq('array joins', T.html`${[1, 2].map(n => T.html`<li>${n}</li>`)}`.__html, '<li>1</li><li>2</li>');

console.log(passed + ' passed, ' + failed + ' failed');
process.exit(failed ? 1 : 0);
