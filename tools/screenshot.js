#!/usr/bin/env node
/* Screenshots of the web UI, e.g. for the README: drives a headless Chrome
 * over the DevTools protocol so a phone-sized viewport can be emulated
 * (Chrome's own --screenshot cannot open a window narrower than ~500 px and
 * stops the page - closing the websocket - before it captures).
 *
 *   node tools/mock-device.js &
 *   node tools/screenshot.js http://localhost:8285/#/dashboard images/webgui.png --width 1280 --height 900
 *   node tools/screenshot.js http://localhost:8285/#/senddata images/webgui-phone.png --width 390 --height 844 --mobile
 *
 * Options: --width --height (CSS px), --scale (device pixel ratio, default 2),
 * --mobile (touch + mobile UA), --dark (prefers-color-scheme: dark),
 * --wait ms (default 4000, time for the websocket and readings to arrive).
 * CHROME=/path/to/chrome overrides the binary. */
'use strict';
const { spawn } = require('child_process');
const http = require('http');
const fs = require('fs');
const os = require('os');
const path = require('path');

const args = process.argv.slice(2);
const positional = args.filter(a => !a.startsWith('--'));
const flag = name => args.includes('--' + name);
const opt = (name, def) => { const i = args.indexOf('--' + name); return i >= 0 && args[i + 1] ? args[i + 1] : def; };
const [url, out] = positional;
if (!url || !out) { console.error('usage: screenshot.js <url> <out.png> [--width N --height N --scale N --mobile --dark --wait ms]'); process.exit(2); }
const width = parseInt(opt('width', '1280'), 10), height = parseInt(opt('height', '900'), 10);
const scale = parseFloat(opt('scale', '2')), wait = parseInt(opt('wait', '4000'), 10);
const CHROME = process.env.CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const port = 9222 + Math.floor(Math.random() * 500);
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'tehybug-shot-'));
const sleep = ms => new Promise(r => setTimeout(r, ms));
const getJson = (u, method) => new Promise((res, rej) => {
  const req = http.request(u, { method: method || 'GET' }, r => { let b = ''; r.on('data', c => b += c); r.on('end', () => { try { res(JSON.parse(b)); } catch (e) { rej(new Error('bad json from ' + u + ': ' + b.slice(0, 100))); } }); });
  req.on('error', rej); req.end();
});

(async () => {
  // Chrome's stderr goes to a file: with stdio ignored this (Rosetta) build
  // never came up on the debugging port
  const log = fs.openSync(path.join(profile, 'chrome.log'), 'w');
  const chrome = spawn(CHROME, ['--headless=new', '--no-first-run', '--no-default-browser-check', '--disable-extensions', '--disable-gpu', '--hide-scrollbars', '--force-color-profile=srgb', '--remote-debugging-port=' + port, '--user-data-dir=' + profile, 'about:blank'], { stdio: ['ignore', log, log] });
  const cleanup = () => { try { chrome.kill('SIGKILL'); } catch (e) { /* gone */ } try { fs.rmSync(profile, { recursive: true, force: true }); } catch (e) { /* */ } };
  process.on('exit', cleanup);
  try {
    let target = null, lastError = '';
    for (let i = 0; i < 75 && !target; i++) { await sleep(200); try { target = await getJson('http://127.0.0.1:' + port + '/json/new?about:blank', 'PUT'); } catch (e) { lastError = e.message; } }
    if (!target) throw new Error('Chrome did not come up on port ' + port + ' (' + lastError + ')');
    const ws = new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((res, rej) => { ws.onopen = res; ws.onerror = () => rej(new Error('devtools socket failed')); });
    let seq = 0; const pending = new Map();
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id && pending.has(m.id)) { const p = pending.get(m.id); pending.delete(m.id); m.error ? p.reject(new Error(m.error.message)) : p.resolve(m.result); } };
    const send = (method, params) => new Promise((resolve, reject) => { const id = ++seq; pending.set(id, { resolve, reject }); ws.send(JSON.stringify({ id, method, params: params || {} })); });
    await send('Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: scale, mobile: flag('mobile') });
    if (flag('mobile')) await send('Emulation.setTouchEmulationEnabled', { enabled: true });
    if (flag('dark')) await send('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-color-scheme', value: 'dark' }] });
    await send('Page.enable');
    await send('Page.navigate', { url });
    await sleep(wait);
    const shot = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false });
    fs.writeFileSync(out, Buffer.from(shot.data, 'base64'));
    console.log('wrote ' + out + ' (' + width + 'x' + height + ' @' + scale + 'x)');
    ws.close();
  } finally { cleanup(); }
})().catch(e => { console.error(e.message); process.exit(1); });
