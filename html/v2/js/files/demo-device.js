/* Demo device: stands in for a TeHyBug so the interface can be tried
 * without hardware (demo.html). Replaces the HTTP and websocket transports
 * with an in-page simulation; nothing else in the UI knows the difference.
 * Not part of the bundle — demo.html loads it after javascript.php. */
(function () {
  'use strict';
  const T = window.TeHyBug;
  if (!T || !window.TEHYBUG_DEMO) return;
  const params = new URLSearchParams(location.search);
  const board = params.get('board') || 'universal';
  const t0 = Date.now();
  const config = {
    key: 'a1b2c3d4e5f6', board,
    mqttActive: false, mqttRetained: false, mqttUser: '', mqttPassword: '', mqttServer: '0.0.0.0', mqttMasterTopic: '/tehybug', mqttMessage: '', mqttPort: 1883, mqttFrequency: 900,
    haActive: false,
    eepromLogActive: false, eepromLogFrequency: 60, eepromLogMessage: '', eepromLogHourly: false, offlineModeActive: false,
    httpGetURL: '', httpGetActive: false, httpGetFrequency: 900,
    httpPostURL: '', httpPostActive: false, httpPostFrequency: 900, httpPostJson: '', httpsFingerprint: '', ntpActive: true, ntpServer: 'pool.ntp.org', timezone: '',
    calibrationActive: false, calibrationTemp: 0, calibrationHumi: 0, calibrationQfe: 0,
    configModeActive: true, sleepModeActive: false, lightSleepModeActive: false,
    dht_sensor: false, second_dht_sensor: false, ds18b20_sensor: false, second_ds18b20_sensor: false, adc_sensor: false,
    rc_active: false, rc_url: '',
    line1: '%temp% °C', line2: '%humi% %RH', line3: '%qfe% hPa', clock_12h: false, clock_show_ip: true, clock_sleep: false, clock_sleep_start: '22:00', clock_sleep_finish: '07:00'
  };
  for (let i = 1; i <= 3; i++) {
    Object.assign(config, { ['sc' + i + '_active']: false, ['sc' + i + '_type']: 'get', ['sc' + i + '_url']: '', ['sc' + i + '_data']: 'temp', ['sc' + i + '_condition']: 'gt', ['sc' + i + '_value']: 0, ['sc' + i + '_message']: '' });
    Object.assign(config, { ['alarm' + i + 'Active']: false, ['alarm' + i + 'Time']: '', ['alarm' + i + 'Message']: '', ['alarm' + i + 'Weekdays']: '0,0,0,0,0,0,0' });
  }
  const info = () => ({
    gumboardVersion: '1.0.0', fwBuild: '2609021200', board,
    sketchSize: 561232, freeSketchSpace: 1449984, wifiRSSI: '-61', wifiQuality: 78, wifiSSID: 'Demo WiFi', ipAddress: '192.168.1.42',
    freeHeap: 24816, chipID: 1054321, cpuFreqMHz: 80, sleepModeActive: config.sleepModeActive, deepSleepMax: 12884, key: config.key,
    uptimeS: Math.round((Date.now() - t0) / 1000),
    detected: { bmx: true, bme680: false, aht20: false, am2320: false, max44009: false, sgp30: false, ds3231: true, eeprom: true }
  });
  let clockSet = true;
  const wobble = (v, r) => (v + (Math.random() - 0.5) * r).toFixed(1);
  function sensors() {
    const temp = +wobble(22.4, 0.4), humi = +wobble(47.5, 1.5);
    const out = { key: config.key, temp: temp.toFixed(1), temp_imp: (temp * 9 / 5 + 32).toFixed(1), humi: humi.toFixed(1), qfe: wobble(1013.2, 0.6), alt: '12.0', dew: '10.6', dew_imp: '51.1', hi: '22.1', hi_imp: '71.8', ah: '9.4', cr: '92.0', cs: '0' };
    if (config.adc_sensor) out.adc = String(Math.round(512 + Math.random() * 20));
    if (config.second_dht_sensor) { out.temp2 = wobble(19.8, 0.4); out.temp2_imp = '67.6'; out.humi2 = wobble(55.2, 1.5); }
    if (config.second_ds18b20_sensor) { out.temp2 = wobble(4.2, 0.3); out.temp2_imp = '39.6'; }
    return out;
  }
  let down = false;
  const busy = () => Promise.reject(new Error('Failed to fetch'));
  const later = (v, ms) => new Promise(res => setTimeout(() => res(v), ms || 150));
  function simulateReboot() {
    down = true;
    T.pushLog('SetConfig', 'restarting');
    T.Live.setOnline(false);
    setTimeout(() => {
      down = false;
      T.Live.setOnline(true);
      T.applyInfo(info());
      T.applySensors(sensors());
      T.pushLog('Setup', 'Webserver started');
    }, 6000);
  }
  T.Api = {
    info: () => down ? busy() : later(info()),
    config: () => down ? busy() : later(Object.assign({}, config)),
    sensor: () => down ? busy() : later(sensors(), 900),
    datalog: () => down ? busy() : later({ active: true, timeSet: clockSet, time: '2026-09-02 14:05', capacity: 65536, slotBytes: 2031, files: [{ name: '1.txt', size: 812, date: '2026-09-01' }, { name: '2.txt', size: 396, date: '2026-09-02' }] }),
    datalogFile: name => later('07:55 22.6t 48.3h 1013.2p\n08:55 22.8t 47.9h 1013.0p\n09:55 23.1t 47.2h 1012.7p'),
    time: () => later({ rtc: true, timeSet: clockSet, time: '2026-09-02 14:05' }),
    setTime: () => { clockSet = true; return later({ response: 'OK' }); },
    async saveConfig(obj) {
      if (down) throw new Error('Failed to fetch');
      const o = Object.assign({}, obj);
      const reboot = !!o.reboot;
      delete o.reboot;
      Object.assign(config, o);
      T.pushLog('SetConfig', 'Incoming Json length: ' + JSON.stringify(o).length);
      await later(null, 300);
      if (reboot) simulateReboot();
      return { ok: true };
    },
    uploadFirmware: (file, onProgress) => new Promise(res => {
      let p = 0;
      const t = setInterval(() => { p += 0.06; onProgress(Math.min(1, p)); if (p >= 1) { clearInterval(t); simulateReboot(); res('Update Success! Rebooting...'); } }, 120);
    })
  };
  T.Live = {
    start() {
      setTimeout(() => {
        T.Live.setOnline(true);
        T.applyInfo(info());
        T.applySensors(sensors());
        T.pushLog('WebSocketEvent', '[0] Connected from 192.168.1.20 url: /main');
      }, 400);
      setInterval(() => { if (!down && T.State.online) T.pushLog('Demo', 'heartbeat — a real device logs its sends and errors here'); }, 20000);
    },
    stop() { T.Live.setOnline(false); },
    setOnline(on) { if (T.State.online === on) return; T.State.online = on; T.Bus.emit(on ? 'online' : 'offline'); }
  };
})();
