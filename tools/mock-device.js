#!/usr/bin/env node
/* A fake TeHyBug for developing the web UI (html/v2) against the real
 * transport: the same HTTP API and websocket the firmware exposes, serving
 * the same bootstrap page — with the assets read from the working tree
 * instead of tehybug.com. No dependencies; the websocket server is the
 * few dozen lines the protocol needs for text frames.
 *
 *   node tools/mock-device.js [--port 8285] [--board universal|display|generic]
 *
 * then open http://localhost:8285/ . POST /api/config with reboot:true makes
 * the mock "restart": it drops every connection and answers nothing for
 * eight seconds, like the device.
 */
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const args = process.argv.slice(2);
const opt = (name, def) => { const i = args.indexOf('--' + name); return i >= 0 && args[i + 1] ? args[i + 1] : def; };
const PORT = parseInt(opt('port', '8285'), 10);
const WS_PORT = PORT + 1;
const BOARD = opt('board', 'universal');
const ROOT = path.join(__dirname, '..', 'html', 'v2');
const manifest = JSON.parse(fs.readFileSync(path.join(ROOT, 'manifest.json'), 'utf8'));

const config = {
  key: 'mock00112233', mqttActive: false, mqttRetained: false, mqttUser: '', mqttPassword: '', mqttServer: '0.0.0.0', mqttMasterTopic: '/tehybug', mqttMessage: '', mqttPort: 1883, mqttFrequency: 900,
  haActive: false, eepromLogActive: false, eepromLogFrequency: 60, eepromLogMessage: '', eepromLogHourly: false, offlineModeActive: false,
  httpGetURL: '', httpGetActive: false, httpGetFrequency: 900, httpPostURL: '', httpPostActive: false, httpPostFrequency: 900, httpPostJson: '', httpsFingerprint: '',
  calibrationActive: false, calibrationTemp: 0, calibrationHumi: 0, calibrationQfe: 0, configModeActive: true, sleepModeActive: false, lightSleepModeActive: false,
  dht_sensor: false, second_dht_sensor: false, ds18b20_sensor: false, second_ds18b20_sensor: false, adc_sensor: false, rc_active: false, rc_url: ''
};
for (let i = 1; i <= 3; i++) {
  Object.assign(config, { ['sc' + i + '_active']: false, ['sc' + i + '_type']: 'get', ['sc' + i + '_url']: '', ['sc' + i + '_data']: 'temp', ['sc' + i + '_condition']: 'gt', ['sc' + i + '_value']: 0, ['sc' + i + '_message']: '' });
}
if (BOARD === 'display') {
  Object.assign(config, { line1: '%temp% °C', line2: '%humi% %RH', line3: '%qfe% hPa', clock_12h: false, clock_show_ip: true, clock_sleep: false, clock_sleep_start: '22:00', clock_sleep_finish: '07:00' });
  for (let i = 1; i <= 3; i++) Object.assign(config, { ['alarm' + i + 'Active']: false, ['alarm' + i + 'Time']: '', ['alarm' + i + 'Message']: '', ['alarm' + i + 'Weekdays']: '0,0,0,0,0,0,0' });
}
const t0 = Date.now();
const info = () => ({
  gumboardVersion: '1.0.0', fwBuild: '2609021200', board: BOARD, sketchSize: 561232, freeSketchSpace: 1449984, wifiRSSI: '-58', wifiQuality: 84, wifiSSID: 'MockNet', ipAddress: '127.0.0.1',
  freeHeap: 25000, chipID: 424242, cpuFreqMHz: 80, sleepModeActive: config.sleepModeActive, deepSleepMax: 12884, key: config.key, uptimeS: Math.round((Date.now() - t0) / 1000),
  detected: { bmx: true, bme680: false, aht20: false, am2320: false, max44009: false, sgp30: false, ds3231: true, eeprom: true }
});
const wobble = (v, r) => (v + (Math.random() - 0.5) * r).toFixed(1);
const sensors = () => ({ key: config.key, temp: wobble(21.7, 0.4), temp_imp: '71.1', humi: wobble(49.0, 1.5), qfe: wobble(1009.8, 0.6), alt: '40.0', dew: '10.6', dew_imp: '51.1', hi: '21.5', hi_imp: '70.7', ah: '9.4', cr: '95.0', cs: '0' });
let rebootingUntil = 0;
let clockSet = false;

/* ---- websocket server (text frames only) ---- */
const clients = new Set();
function encodeFrame(op, payload) {
  const len = payload.length;
  let header;
  if (len < 126) header = Buffer.from([0x80 | op, len]);
  else if (len < 65536) { header = Buffer.alloc(4); header[0] = 0x80 | op; header[1] = 126; header.writeUInt16BE(len, 2); }
  else { header = Buffer.alloc(10); header[0] = 0x80 | op; header[1] = 127; header.writeBigUInt64BE(BigInt(len), 2); }
  return Buffer.concat([header, payload]);
}
function parseFrame(buf) {
  if (buf.length < 2) return null;
  const op = buf[0] & 0x0f, masked = buf[1] & 0x80;
  let len = buf[1] & 0x7f, off = 2;
  if (len === 126) { if (buf.length < 4) return null; len = buf.readUInt16BE(2); off = 4; }
  else if (len === 127) { if (buf.length < 10) return null; len = Number(buf.readBigUInt64BE(2)); off = 10; }
  const maskLen = masked ? 4 : 0;
  if (buf.length < off + maskLen + len) return null;
  const payload = Buffer.from(buf.subarray(off + maskLen, off + maskLen + len));
  if (masked) { const mask = buf.subarray(off, off + 4); for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i % 4]; }
  return { op, payload, length: off + maskLen + len };
}
function send(client, text) { try { client.socket.write(encodeFrame(1, Buffer.from(text))); } catch (e) { /* gone */ } }
function broadcast(obj, paths) { clients.forEach(c => { if (!paths || paths.indexOf(c.path) >= 0) send(c, JSON.stringify(obj)); }); }
function log(fn, message) { console.log('[mock] ' + fn + ': ' + message); broadcast({ log: { function: fn, message } }, ['/main']); }
const wsServer = http.createServer((req, res) => { res.writeHead(426); res.end('websocket only'); });
wsServer.on('upgrade', (req, socket) => {
  const key = req.headers['sec-websocket-key'];
  if (!key) { socket.destroy(); return; }
  const accept = crypto.createHash('sha1').update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  socket.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ' + accept + '\r\n\r\n');
  const client = { socket, path: req.url };
  clients.add(client);
  let buf = Buffer.alloc(0);
  socket.on('data', chunk => {
    buf = Buffer.concat([buf, chunk]);
    for (;;) {
      const f = parseFrame(buf);
      if (!f) break;
      buf = buf.subarray(f.length);
      if (f.op === 8) { try { socket.end(); } catch (e) { /* */ } clients.delete(client); }
      else if (f.op === 9) socket.write(encodeFrame(10, f.payload));
      else if (f.op === 1) {
        const text = f.payload.toString();
        if (text[0] === '{' && client.path === '/setConfig') { try { applyConfig(JSON.parse(text)); } catch (e) { log('WebSocketEvent', 'Ignored a config that could not be parsed'); } }
      }
    }
  });
  socket.on('close', () => clients.delete(client));
  socket.on('error', () => clients.delete(client));
  log('WebSocketEvent', '[' + clients.size + '] Connected from ' + socket.remoteAddress + ' url: ' + req.url);
  // exactly what the firmware pushes on connect for these paths
  if (['/main', '/firststart', '/api/info', '/settings', '/firmware'].indexOf(req.url) >= 0) send(client, JSON.stringify(info()));
  if (['/main', '/settings', '/datalog', '/display_settings'].indexOf(req.url) >= 0) send(client, JSON.stringify(sensors()));
  if (['/settings', '/setsensor', '/scenarios', '/setsystem', '/datalog', '/display_settings'].indexOf(req.url) >= 0) send(client, JSON.stringify(Object.assign({ board: BOARD }, config)));
});
wsServer.listen(WS_PORT);

function applyConfig(obj) {
  const reboot = !!obj.reboot;
  Object.keys(obj).forEach(k => { if (k !== 'reboot' && k in config) config[k] = obj[k]; });
  log('SetConfig', 'Incoming Json length: ' + JSON.stringify(obj).length);
  if (reboot) setTimeout(() => {
    console.log('[mock] "restarting" for 8 s');
    rebootingUntil = Date.now() + 8000;
    clients.forEach(c => { try { c.socket.destroy(); } catch (e) { /* */ } });
    clients.clear();
  }, 300);
}

/* ---- HTTP ---- */
// The same inline styles the firmware page carries (src/web_api.h mainPage),
// plus the older firmware's #page rule: the UI renders into #page, so
// anything either page does to it must be visible here, not only on a real
// device. Keep this in step with mainPage whenever it changes.
const bootstrap = () => `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover"><meta name="theme-color" content="#0f7a58"><title>TeHyBug (mock)</title>
<style>
body{margin:0;font:16px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:#eef1f4;color:#182029}
#page{max-width:520px;margin:0 auto;padding:24px 16px}
.hello{box-sizing:border-box;width:calc(100% - 32px);max-width:520px;margin:24px auto;background:#fff;border:1px solid #dde3e9;border-radius:14px;padding:20px}
.hello h1{font-size:1.3rem;margin:0 0 8px;color:#0f7a58}
</style></head>
<body><div id="page"><div class="hello"><h1>TeHyBug</h1><p>On your own network this device is at <b><span id="ip">tehybug.local</span></b>.</p><p>Loading the full interface…</p></div></div>
<script>window.TEHYBUG_WS_PORT=${WS_PORT};</script>
<link rel="stylesheet" href="/v2/css/style.php" media="print" onload="this.media='all'">
<script src="/v2/js/javascript.php" defer></script>
</body></html>`;
const TYPES = { '.js': 'text/javascript', '.css': 'text/css', '.html': 'text/html', '.json': 'application/json' };
function bundle(kind) {
  const sep = kind === 'js' ? '\n;\n' : '\n';
  return manifest[kind].map(f => fs.readFileSync(path.join(ROOT, kind, f), 'utf8') + sep).join('');
}
// no-store: the browser must not keep an old bundle across edits to html/v2
function json(res, obj, code) { res.writeHead(code || 200, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store', Connection: 'close' }); res.end(JSON.stringify(obj)); }
function text(res, body, type, code) { res.writeHead(code || 200, { 'Content-Type': type || 'text/plain', 'Cache-Control': 'no-store', Connection: 'close' }); res.end(body); }
function readBody(req) { return new Promise(res => { const chunks = []; req.on('data', c => chunks.push(c)); req.on('end', () => res(Buffer.concat(chunks))); }); }

http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://localhost');
  const p = url.pathname;
  if (Date.now() < rebootingUntil) { req.socket.destroy(); return; }
  if (p === '/') return text(res, bootstrap(), 'text/html');
  if (p === '/v2/css/style.php') return text(res, bundle('css'), 'text/css');
  if (p === '/v2/js/javascript.php') return text(res, bundle('js'), 'text/javascript');
  if (p.startsWith('/v2/')) {
    const file = path.join(ROOT, p.slice(4));
    if (file.startsWith(ROOT) && fs.existsSync(file) && fs.statSync(file).isFile()) return text(res, fs.readFileSync(file), TYPES[path.extname(file)] || 'application/octet-stream');
    return text(res, 'not found', 'text/plain', 404);
  }
  if (p === '/api/info') return json(res, info());
  if (p === '/api/config' && req.method === 'GET') return json(res, Object.assign({ board: BOARD }, config));
  if (p === '/api/config' && req.method === 'POST') {
    let obj;
    try { obj = JSON.parse((await readBody(req)).toString()); } catch (e) { return json(res, { response: 'Not Acceptable' }, 406); }
    applyConfig(obj);
    return json(res, obj.reboot ? { response: 'OK', reboot: true } : { response: 'OK' });
  }
  if (p === '/api/sensor') { await new Promise(r => setTimeout(r, 700)); const s = sensors(); broadcast(s, ['/main', '/settings', '/datalog', '/display_settings']); return json(res, s); }
  if (p === '/api/datalog') {
    if (url.searchParams.has('file')) return text(res, '07:55 22.6t 48.3h 1013.2p\n08:55 22.8t 47.9h 1013.0p\n');
    return json(res, { active: true, timeSet: clockSet, time: '2026-09-02 14:05', capacity: 65536, slotBytes: 2031, files: [{ name: '1.txt', size: 812, date: '2026-09-01' }, { name: '2.txt', size: 396, date: '2026-09-02' }] });
  }
  if (p === '/api/time') return json(res, { rtc: true, timeSet: clockSet, time: '2026-09-02 14:05' });
  if (p === '/api/settime') { clockSet = true; return json(res, { response: 'OK', time: '2026-09-02 14:05' }); }
  if (p === '/api/getip') return text(res, '127.0.0.1', 'text/html');
  if (p === '/update' && req.method === 'GET') return text(res, "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='firmware'><input type='submit' value='Update'></form>", 'text/html');
  if (p === '/update' && req.method === 'POST') {
    const body = await readBody(req);
    log('Update', 'received ' + body.length + ' bytes');
    if (url.searchParams.has('fail')) return text(res, 'Update error: not enough space', 'text/html');
    applyConfig({ reboot: true });
    return text(res, '<META http-equiv="refresh" content="15;URL=/">Update Success! Rebooting...', 'text/html');
  }
  res.writeHead(302, { Location: '/update' });
  res.end();
}).listen(PORT, () => console.log('[mock] TeHyBug (' + BOARD + ') at http://localhost:' + PORT + '/  (websocket on ' + WS_PORT + ')'));
