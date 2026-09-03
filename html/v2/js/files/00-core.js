/* TeHyBug web UI v2 — core.
 *
 * State, transport (HTTP API + websocket), the reading catalogue and the
 * page registry. The device serves a one-screen page (src/web_api.h) that
 * loads this bundle from tehybug.com; every /api call and the websocket are
 * same-origin with that page, so nothing here needs CORS. Each module is an
 * IIFE hanging off window.TeHyBug; the bundle order is manifest.json.
 */
(function () {
  'use strict';
  const T = window.TeHyBug = window.TeHyBug || {};
  T.UI_VERSION = '2.0.0';
  T.CLOUD_URL = 'http://tehybug.com/track/';
  T.AP_PASSWORD = 'TeHyBug123';
  T.REPO = 'https://github.com/gumslone/tehybug-universal';

  /* ---------------- DOM & templating ---------------- */
  const $ = (sel, root) => (root || document).querySelector(sel);
  const $$ = (sel, root) => Array.prototype.slice.call((root || document).querySelectorAll(sel));
  function esc(s) {
    return String(s == null ? '' : s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  }
  function raw(s) { return { __html: String(s == null ? '' : s) }; }
  function isRaw(v) { return !!v && typeof v === 'object' && Object.prototype.hasOwnProperty.call(v, '__html'); }
  function part(v) {
    if (v == null || v === false || v === true) return '';
    if (isRaw(v)) return v.__html;
    if (Array.isArray(v)) return v.map(part).join('');
    return esc(v);
  }
  // Tagged template: every interpolation is HTML-escaped unless it is the
  // result of another html`` call (or raw()). Device-provided strings — an
  // SSID, a stored URL — therefore never become markup.
  function html(strings) {
    let out = '';
    for (let i = 0; i < strings.length; i++) {
      out += strings[i];
      if (i + 1 < arguments.length) out += part(arguments[i + 1]);
    }
    return raw(out);
  }
  function render(el, tpl) { if (el) el.innerHTML = isRaw(tpl) ? tpl.__html : esc(tpl); }
  Object.assign(T, { $, $$, esc, raw, html, render, isRaw });
  T.sleep = ms => new Promise(res => setTimeout(res, ms));

  /* ---------------- Icons (inline SVG, feather-style) ---------------- */
  const ICONS = {
    menu: '<line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="18" x2="21" y2="18"/>',
    x: '<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>',
    home: '<path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><polyline points="9 22 9 12 15 12 15 22"/>',
    play: '<polygon points="5 3 19 12 5 21 5 3"/>',
    'book-open': '<path d="M2 3h6a4 4 0 0 1 4 4v14a3 3 0 0 0-3-3H2z"/><path d="M22 3h-6a4 4 0 0 0-4 4v14a3 3 0 0 1 3-3h7z"/>',
    thermometer: '<path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z"/>',
    droplet: '<path d="M12 2.69l5.66 5.66a8 8 0 1 1-11.31 0z"/>',
    sun: '<circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/>',
    wind: '<path d="M9.59 4.59A2 2 0 1 1 11 8H2m10.59 11.41A2 2 0 1 0 14 16H2m15.73-8.27A2.5 2.5 0 1 1 19.5 12H2"/>',
    activity: '<polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/>',
    send: '<line x1="22" y1="2" x2="11" y2="13"/><polygon points="22 2 15 22 11 13 2 9 22 2"/>',
    cloud: '<path d="M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z"/>',
    zap: '<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>',
    power: '<path d="M18.36 6.64a9 9 0 1 1-12.73 0"/><line x1="12" y1="2" x2="12" y2="12"/>',
    layers: '<polygon points="12 2 2 7 12 12 22 7 12 2"/><polyline points="2 17 12 22 22 17"/><polyline points="2 12 12 17 22 12"/>',
    'hard-drive': '<line x1="22" y1="12" x2="2" y2="12"/><path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/><line x1="6" y1="16" x2="6.01" y2="16"/><line x1="10" y1="16" x2="10.01" y2="16"/>',
    monitor: '<rect x="2" y="3" width="20" height="14" rx="2" ry="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/>',
    download: '<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/>',
    upload: '<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/>',
    'help-circle': '<circle cx="12" cy="12" r="10"/><path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3"/><line x1="12" y1="17" x2="12.01" y2="17"/>',
    wifi: '<path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M1.42 9a16 16 0 0 1 21.16 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><line x1="12" y1="20" x2="12.01" y2="20"/>',
    'wifi-off': '<line x1="1" y1="1" x2="23" y2="23"/><path d="M16.72 11.06A10.94 10.94 0 0 1 19 12.55"/><path d="M5 12.55a10.94 10.94 0 0 1 5.17-2.39"/><path d="M10.71 5.05A16 16 0 0 1 22.58 9"/><path d="M1.42 9a15.91 15.91 0 0 1 4.7-2.88"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><line x1="12" y1="20" x2="12.01" y2="20"/>',
    copy: '<rect x="9" y="9" width="13" height="13" rx="2" ry="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>',
    check: '<polyline points="20 6 9 17 4 12"/>',
    'alert-triangle': '<path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>',
    info: '<circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/>',
    'refresh-cw': '<polyline points="23 4 23 10 17 10"/><polyline points="1 20 1 14 7 14"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/>',
    'rotate-ccw': '<polyline points="1 4 1 10 7 10"/><path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"/>',
    'external-link': '<path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6"/><polyline points="15 3 21 3 21 9"/><line x1="10" y1="14" x2="21" y2="3"/>',
    'chevron-right': '<polyline points="9 18 15 12 9 6"/>',
    'chevron-down': '<polyline points="6 9 12 15 18 9"/>',
    'arrow-right': '<line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/>',
    clock: '<circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>',
    save: '<path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/>',
    cpu: '<rect x="4" y="4" width="16" height="16" rx="2" ry="2"/><rect x="9" y="9" width="6" height="6"/><line x1="9" y1="1" x2="9" y2="4"/><line x1="15" y1="1" x2="15" y2="4"/><line x1="9" y1="20" x2="9" y2="23"/><line x1="15" y1="20" x2="15" y2="23"/><line x1="20" y1="9" x2="23" y2="9"/><line x1="20" y1="14" x2="23" y2="14"/><line x1="1" y1="9" x2="4" y2="9"/><line x1="1" y1="14" x2="4" y2="14"/>',
    settings: '<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"/>',
    sliders: '<line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/>',
    terminal: '<polyline points="4 17 10 11 4 5"/><line x1="12" y1="19" x2="20" y2="19"/>',
    eye: '<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/>',
    radio: '<circle cx="12" cy="12" r="2"/><path d="M16.24 7.76a6 6 0 0 1 0 8.49m-8.48-.01a6 6 0 0 1 0-8.49m11.31-2.82a10 10 0 0 1 0 14.14m-14.14 0a10 10 0 0 1 0-14.14"/>',
    database: '<ellipse cx="12" cy="5" rx="9" ry="3"/><path d="M21 12c0 1.66-4 3-9 3s-9-1.34-9-3"/><path d="M3 5v14c0 1.66 4 3 9 3s9-1.34 9-3V5"/>',
    list: '<line x1="8" y1="6" x2="21" y2="6"/><line x1="8" y1="12" x2="21" y2="12"/><line x1="8" y1="18" x2="21" y2="18"/><line x1="3" y1="6" x2="3.01" y2="6"/><line x1="3" y1="12" x2="3.01" y2="12"/><line x1="3" y1="18" x2="3.01" y2="18"/>',
    key: '<path d="M21 2l-2 2m-7.61 7.61a5.5 5.5 0 1 1-7.778 7.778 5.5 5.5 0 0 1 7.777-7.777zm0 0L15.5 7.5m0 0l3 3L22 7l-3-3m-3.5 3.5L19 4"/>',
    moon: '<path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>',
    bell: '<path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"/><path d="M13.73 21a2 2 0 0 1-3.46 0"/>',
    'file-text': '<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/>',
    battery: '<rect x="1" y="6" width="18" height="12" rx="2" ry="2"/><line x1="23" y1="13" x2="23" y2="11"/>',
    globe: '<circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>',
    github: '<path d="M9 19c-5 1.5-5-2.5-7-3m14 6v-3.87a3.37 3.37 0 0 0-.94-2.61c3.14-.35 6.44-1.54 6.44-7A5.44 5.44 0 0 0 20 4.77 5.07 5.07 0 0 0 19.91 1S18.73.65 16 2.48a13.38 13.38 0 0 0-7 0C6.27.65 5.09 1 5.09 1A5.07 5.07 0 0 0 5 4.77a5.44 5.44 0 0 0-1.5 3.78c0 5.42 3.3 6.61 6.44 7A3.37 3.37 0 0 0 9 18.13V22"/>',
    'shopping-cart': '<circle cx="9" cy="21" r="1"/><circle cx="20" cy="21" r="1"/><path d="M1 1h4l2.68 13.39a2 2 0 0 0 2 1.61h9.72a2 2 0 0 0 2-1.61L23 6H6"/>',
    'trash-2': '<polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/>'
  };
  // An LED colour as the device shows it: a dot in that colour plus the word
  T.led = colour => raw('<span class="led led-' + esc(colour) + '">' + esc(colour) + '</span>');
  T.icon = (name, cls) => raw('<svg class="i' + (cls ? ' ' + cls : '') + '" viewBox="0 0 24 24" aria-hidden="true">' + (ICONS[name] || '') + '</svg>');

  /* ---------------- Events & state ---------------- */
  const listeners = {};
  T.Bus = {
    on(evt, fn) { (listeners[evt] = listeners[evt] || []).push(fn); return () => T.Bus.off(evt, fn); },
    off(evt, fn) { listeners[evt] = (listeners[evt] || []).filter(f => f !== fn); },
    emit(evt, data) {
      (listeners[evt] || []).slice().forEach(fn => { try { fn(data); } catch (e) { console.error(evt, e); } });
    }
  };

  T.State = {
    info: {},        // /api/info (also pushed on websocket connect)
    config: {},      // /api/config, the full configuration dump
    sensors: {},     // latest readings by key, as the device sends them (strings)
    seen: {},        // every reading key the device has produced this session
    sensorsAt: 0,
    online: false,   // websocket connected
    infoLoaded: false,
    configLoaded: false,
    log: []
  };
  T.board = () => String(T.State.info.board || T.State.config.board || '');
  T.isDisplay = () => T.board() === 'display';
  T.isGeneric = () => T.board() === 'generic';
  T.boardName = () => ({
    universal: 'TeHyBug universal / Mini',
    display: 'TeHyBug Display Weatherstation',
    generic: 'First-generation TeHyBug'
  }[T.board()] || (T.board() ? 'TeHyBug (' + T.board() + ')' : 'TeHyBug'));
  // the firmware build file this board runs, as named in firmware/ and the releases
  T.buildName = () => ({ universal: 'esp8285', display: 'display', generic: 'generic' }[T.board()] || '');

  T.Store = {
    get(k, d) { try { const v = localStorage.getItem('tehybug.' + k); return v == null ? d : JSON.parse(v); } catch (e) { return d; } },
    set(k, v) { try { localStorage.setItem('tehybug.' + k, JSON.stringify(v)); } catch (e) { /* private mode */ } }
  };
  T.units = () => T.Store.get('units', 'metric');

  /* ---------------- Reading catalogue ---------------- */
  // key, name, unit, icon, short GET parameter (null = JSON/MQTT only).
  // The GET parameters match what tehybug.com/track expects; the order is
  // the display order on the dashboard and in generated payloads.
  const READINGS = [
    ['temp', 'Temperature', '°C', 'thermometer', 't'],
    ['temp2', 'Temperature 2', '°C', 'thermometer', 't2'],
    ['humi', 'Humidity', '%RH', 'droplet', 'h'],
    ['humi2', 'Humidity 2', '%RH', 'droplet', 'h2'],
    ['qfe', 'Pressure', 'hPa', 'activity', 'p'],
    ['alt', 'Altitude', 'm', 'activity', null],
    ['dew', 'Dew point', '°C', 'droplet', null],
    ['dew2', 'Dew point 2', '°C', 'droplet', null],
    ['hi', 'Heat index', '°C', 'thermometer', null],
    ['hi2', 'Heat index 2', '°C', 'thermometer', null],
    ['ah', 'Absolute humidity', 'g/m³', 'droplet', 'ah'],
    ['ah2', 'Absolute humidity 2', 'g/m³', 'droplet', null],
    ['cr', 'Comfort ratio', '%', 'sun', null],
    ['cr2', 'Comfort ratio 2', '%', 'sun', null],
    ['cs', 'Comfort', '', 'sun', null],
    ['cs2', 'Comfort 2', '', 'sun', null],
    ['air', 'Gas resistance', 'kΩ', 'wind', 'a'],
    ['iaq', 'Air quality (IAQ)', '', 'wind', null],
    ['eco2', 'CO₂ equivalent', 'ppm', 'wind', null],
    ['bvoc', 'VOC equivalent', 'ppm', 'wind', null],
    ['tvoc', 'Total VOC', 'ppb', 'wind', null],
    ['uv', 'UV index', '', 'sun', 'u'],
    ['lux', 'Light', 'lux', 'sun', 'l'],
    ['adc', 'Analog input (ADC)', '', 'activity', 'x']
  ];
  const MAP = {};
  READINGS.forEach(r => { MAP[r[0]] = { key: r[0], name: r[1], unit: r[2], icon: r[3], param: r[4] }; });
  // DHTesp's ComfortState enum, which the firmware reports as an integer. It
  // is a bit field (hot 1, cold 2, dry 4, humid 8), not a sequence.
  const COMFORT = { 0: 'Comfortable', 1: 'Too hot', 2: 'Too cold', 4: 'Too dry', 8: 'Too humid', 9: 'Hot and humid', 5: 'Hot and dry', 10: 'Cold and humid', 6: 'Cold and dry' };
  // The firmware names Fahrenheit siblings inconsistently: temp -> temp_imp,
  // temp2 -> temp2_imp, but hi2 -> hi_imp2 and dew2 -> dew_imp2.
  function impKey(k) {
    const m = /^(temp|hi|dew)(2?)$/.exec(k);
    if (!m) return null;
    return m[1] === 'temp' ? k + '_imp' : m[1] + '_imp' + m[2];
  }
  function baseKey(k) {
    let m = /^(temp|hi|dew)(2?)_imp$/.exec(k);
    if (m) return m[1] + m[2];
    m = /^(hi|dew)_imp(2)$/.exec(k);
    if (m) return m[1] + m[2];
    return k;
  }
  T.Readings = {
    order: READINGS.map(r => r[0]),
    def: k => MAP[baseKey(k)],
    isReading: k => !!MAP[baseKey(k)],
    baseKey,
    impKey,
    isImperial: k => /_imp/.test(k),
    // the key to show for `k` in the chosen unit system, if the device makes it
    keyFor(k, units) {
      if (units === 'imperial') { const i = impKey(k); if (i && T.State.seen[i]) return i; }
      return k;
    },
    unit(k) { if (/_imp/.test(k)) return '°F'; const d = MAP[baseKey(k)]; return d ? d.unit : ''; },
    name(k) { const d = MAP[baseKey(k)]; return d ? d.name : k; },
    icon(k) { const d = MAP[baseKey(k)]; return d ? d.icon : 'activity'; },
    // metric keys the device has reported, in catalogue order, then any
    // key the catalogue does not know (future firmware) after them
    known() {
      const known = READINGS.map(r => r[0]).filter(k => T.State.seen[k]);
      const extra = Object.keys(T.State.seen).filter(k => !MAP[baseKey(k)]).sort();
      return known.concat(extra);
    },
    value(k) {
      const v = T.State.sensors[k];
      if (v == null) return '';
      if (/^cs2?$/.test(k)) return COMFORT[parseInt(v, 10)] || String(v);
      return String(v);
    },
    isText: k => /^cs2?$/.test(k)
  };

  // Suggested query strings / payloads built from what this device really
  // measures, instead of a generic example that names sensors it lacks.
  T.Suggest = {
    keys(units) {
      return T.Readings.known().filter(k => !/^cs2?$/.test(k) && MAP[k]).map(k => T.Readings.keyFor(k, units));
    },
    query(units) {
      const parts = T.Suggest.keys(units).map(k => { const d = MAP[baseKey(k)]; return d && d.param ? '&' + d.param + '=%' + k + '%' : ''; });
      return 'bug_key=%key%' + parts.join('');
    },
    cloudUrl(units) { return T.CLOUD_URL + '?' + T.Suggest.query(units); },
    json(units) {
      const keys = T.Suggest.keys(units);
      return keys.length ? '{' + keys.map(k => '"' + k + '":"%' + k + '%"').join(', ') + '}' : '';
    },
    logTemplate(units) { return T.Suggest.keys(units).map(k => '%' + k + '%').join(' '); },
    have() { return T.Readings.known().some(k => MAP[k]); }
  };

  /* ---------------- HTTP API ---------------- */
  function timeoutFetch(url, opts, ms) {
    const ctl = ('AbortController' in window) ? new AbortController() : null;
    const t = setTimeout(() => { if (ctl) ctl.abort(); }, ms || 8000);
    return fetch(url, Object.assign({ cache: 'no-store', signal: ctl ? ctl.signal : undefined }, opts || {}))
      .finally(() => clearTimeout(t));
  }
  async function getJson(url, ms) {
    const r = await timeoutFetch(url, null, ms);
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return r.json();
  }
  T.Api = {
    info: () => getJson('/api/info', 6000),
    config: () => getJson('/api/config', 8000),
    // reads the sensors on the device; a DHT sample alone takes ~2 s
    sensor: () => getJson('/api/sensor', 15000),
    datalog: () => getJson('/api/datalog', 12000),
    async datalogFile(name) {
      const r = await timeoutFetch('/api/datalog?file=' + encodeURIComponent(name), null, 20000);
      if (!r.ok) throw new Error('HTTP ' + r.status);
      return r.text();
    },
    time: () => getJson('/api/time', 6000),
    // sets the DS3231 from the browser's clock; wd is 1=Sunday..7=Saturday
    setTime() {
      const n = new Date();
      const q = 'y=' + n.getFullYear() + '&mo=' + (n.getMonth() + 1) + '&d=' + n.getDate() +
        '&wd=' + (n.getDay() + 1) + '&h=' + n.getHours() + '&mi=' + n.getMinutes() + '&s=' + n.getSeconds();
      return getJson('/api/settime?' + q, 6000);
    },
    // POSTs a (partial) configuration: the firmware applies only the keys
    // present, so a page sends its own fields and nothing else. Resolves
    // once the device confirms. A network error while `reboot` was
    // requested is reported as unconfirmed rather than failed: firmware
    // from before this UI restarted before answering, and the save most
    // likely went through.
    async saveConfig(obj) {
      let r;
      try {
        r = await timeoutFetch('/api/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(obj)
        }, 12000);
      } catch (e) {
        // only a dropped connection can mean "restarted before answering";
        // an HTTP error status below is a real failure and stays one
        if (obj.reboot) return { ok: true, unconfirmed: true };
        throw e;
      }
      if (!r.ok) {
        throw new Error(r.status === 406 ? 'the device could not parse the settings'
          : r.status === 500 ? 'the device could not write the settings to its flash memory'
          : 'HTTP ' + r.status);
      }
      return { ok: true };
    },
    // OTA: the same multipart POST the firmware's own /update form makes,
    // with upload progress. The updater answers 200 with either
    // "Update Success! Rebooting..." or "Update error: ...".
    uploadFirmware(file, onProgress) {
      return new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest();
        const fd = new FormData();
        fd.append('firmware', file, file.name);
        xhr.open('POST', '/update');
        xhr.upload.onprogress = e => { if (e.lengthComputable && onProgress) onProgress(e.loaded / e.total); };
        xhr.onload = () => {
          const text = String(xhr.responseText || '').replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
          if (xhr.status === 200 && !/error|fail/i.test(text)) resolve(text);
          else reject(new Error(text || ('HTTP ' + xhr.status)));
        };
        xhr.onerror = () => reject(new Error('the connection dropped during the upload'));
        xhr.send(fd);
      });
    }
  };

  /* ---------------- Websocket (live data) ---------------- */
  // One connection for the whole session, on the firmware's "/main" path:
  // it pushes the device info and a sensor read on connect, and every log
  // line after that. Configuration comes over HTTP (the firmware only pushes
  // it to the settings paths, and a fetch is simpler to reason about).
  const Live = T.Live = {
    socket: null,
    wanted: false,
    retryMs: 1500,
    timer: null,
    keepalive: null,
    url() {
      const port = window.TEHYBUG_WS_PORT || 81;
      return 'ws://' + location.hostname + ':' + port + '/main';
    },
    start() { Live.wanted = true; Live.connect(); },
    stop() { Live.wanted = false; clearTimeout(Live.timer); Live.close(); Live.setOnline(false); },
    connect() {
      if (!Live.wanted) return;
      Live.close();
      let ws;
      try { ws = new WebSocket(Live.url()); } catch (e) { Live.scheduleRetry(); return; }
      Live.socket = ws;
      ws.onopen = () => {
        Live.retryMs = 1500;
        Live.setOnline(true);
        // The firmware ignores this text; sending it is what makes a dead
        // TCP connection surface as a close event on the browser side.
        Live.keepalive = setInterval(() => { if (ws.readyState === 1) ws.send('KeepAlive'); }, 5000);
      };
      ws.onclose = () => { if (Live.socket !== ws) return; Live.close(); Live.setOnline(false); Live.scheduleRetry(); };
      ws.onerror = () => { try { ws.close(); } catch (e) { /* already closed */ } };
      ws.onmessage = e => Live.handle(String(e.data));
    },
    close() {
      clearInterval(Live.keepalive);
      Live.keepalive = null;
      const s = Live.socket;
      Live.socket = null;
      if (s) { try { s.onclose = null; s.onmessage = null; s.close(); } catch (e) { /* ignore */ } }
    },
    scheduleRetry() {
      clearTimeout(Live.timer);
      if (!Live.wanted) return;
      Live.timer = setTimeout(Live.connect, Live.retryMs);
      Live.retryMs = Math.min(Math.round(Live.retryMs * 1.5), 8000);
    },
    setOnline(on) {
      if (T.State.online === on) return;
      T.State.online = on;
      T.Bus.emit(on ? 'online' : 'offline');
    },
    handle(text) {
      if (text.charAt(0) !== '{') return;
      let msg;
      try { msg = JSON.parse(text); } catch (e) { return; }
      if (msg.log) { T.pushLog(msg.log.function, msg.log.message); return; }
      if ('gumboardVersion' in msg) { T.applyInfo(msg); return; }
      if ('mqttActive' in msg) { T.applyConfig(msg); return; }
      T.applySensors(msg);
    }
  };

  T.pushLog = (fn, message) => {
    const line = { t: new Date(), fn: String(fn == null ? '' : fn), message: String(message == null ? '' : message) };
    T.State.log.push(line);
    if (T.State.log.length > 500) T.State.log.shift();
    T.Bus.emit('log', line);
  };
  T.applyInfo = info => {
    Object.assign(T.State.info, info);
    T.State.infoLoaded = true;
    T.Bus.emit('info', T.State.info);
  };
  T.applyConfig = cfg => {
    Object.assign(T.State.config, cfg);
    T.State.configLoaded = true;
    T.Bus.emit('config', T.State.config);
  };
  T.applySensors = data => {
    let any = false;
    Object.keys(data || {}).forEach(k => {
      if (k === 'key') return; // the device key rides along for %key%
      T.State.sensors[k] = data[k];
      T.State.seen[k] = true;
      any = true;
    });
    if (any) { T.State.sensorsAt = Date.now(); T.Bus.emit('sensors', T.State.sensors); }
  };

  /* ---------------- Derived facts ---------------- */
  T.hostOf = url => {
    try { return new URL(String(url)).host; } catch (e) { return String(url || '').replace(/^https?:\/\//i, '').split('/')[0]; }
  };
  T.isCloudUrl = url => /tehybug\.com\/track/i.test(String(url || ''));
  // Where the readings go with the current configuration — the one list
  // the dashboard, the go-live dialog and the data pages all agree on.
  T.destinations = () => {
    const c = T.State.config;
    const every = s => 'every ' + T.fmt.secs(s);
    const out = [];
    if (c.httpGetActive && T.isCloudUrl(c.httpGetURL)) out.push({ id: 'cloud', label: 'TeHyBug Cloud', detail: every(c.httpGetFrequency) });
    else if (c.httpGetActive) out.push({ id: 'get', label: 'HTTP GET', detail: T.hostOf(c.httpGetURL) + ' · ' + every(c.httpGetFrequency) });
    if (c.httpPostActive) out.push({ id: 'post', label: 'HTTP POST', detail: T.hostOf(c.httpPostURL) + ' · ' + every(c.httpPostFrequency) });
    if (c.haActive) out.push({ id: 'ha', label: 'Home Assistant', detail: (c.mqttServer || '?') + ' · ' + every(c.mqttFrequency) });
    if (c.mqttActive) out.push({ id: 'mqtt', label: 'MQTT', detail: (c.mqttServer || '?') + ' → ' + (c.mqttMasterTopic || '') + ' · ' + every(c.mqttFrequency) });
    if (c.offlineModeActive) out.push({ id: 'offline', label: 'Offline data log (WiFi off)', detail: every(c.eepromLogFrequency) });
    else if (c.eepromLogActive) out.push({ id: 'eeprom', label: 'On-device data log', detail: every(c.eepromLogFrequency) });
    return out;
  };

  T.fmt = {
    bytes(n) {
      n = Number(n);
      if (!isFinite(n)) return '—';
      if (n < 1024) return n + ' B';
      if (n < 1048576) return (n / 1024).toFixed(1) + ' KB';
      return (n / 1048576).toFixed(2) + ' MB';
    },
    // an interval in seconds as people say it: 900 -> "15 min", 12884 -> "3 h 34 min"
    secs(s) {
      s = Math.round(Number(s));
      if (!isFinite(s)) return '—';
      if (s >= 3600) { const m = Math.round((s % 3600) / 60); return Math.floor(s / 3600) + ' h' + (m ? ' ' + m + ' min' : ''); }
      if (s >= 60) { const r = s % 60; return Math.floor(s / 60) + ' min' + (r ? ' ' + r + ' s' : ''); }
      return s + ' s';
    },
    duration(s) {
      s = Math.floor(Number(s));
      if (!isFinite(s) || s < 0) return '—';
      const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
      if (d) return d + ' d ' + h + ' h';
      if (h) return h + ' h ' + m + ' min';
      if (m) return m + ' min ' + (s % 60) + ' s';
      return s + ' s';
    },
    time(d) { return d.toTimeString().slice(0, 8); }
  };

  // Clipboard: the device page is plain http, where navigator.clipboard is
  // unavailable, so the textarea + execCommand path is the one that counts.
  T.copy = async text => {
    try {
      if (navigator.clipboard && window.isSecureContext) { await navigator.clipboard.writeText(text); return true; }
    } catch (e) { /* fall through */ }
    try {
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.setAttribute('readonly', '');
      ta.style.position = 'fixed';
      ta.style.top = '-1000px';
      document.body.appendChild(ta);
      ta.select();
      ta.setSelectionRange(0, ta.value.length);
      const ok = document.execCommand('copy');
      ta.remove();
      return ok;
    } catch (e) { return false; }
  };

  /* ---------------- Page registry ---------------- */
  // A page: { id, title, nav: {group, icon, order}, boards?, save?, render(),
  //           mount?(root), unmount?(), collect?(), on?: {event: fn} }
  const pages = [];
  const byId = Object.create(null); // no prototype: "#/constructor" must not find one
  T.definePage = def => { pages.push(def); byId[def.id] = def; return def; };
  T.Pages = {
    all: () => pages.slice(),
    get: id => (Object.prototype.hasOwnProperty.call(byId, id) ? byId[id] : undefined),
    visible: () => pages.filter(p => !p.boards || p.boards.indexOf(T.board()) >= 0)
  };
})();
