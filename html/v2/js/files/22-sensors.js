/* Sensors: which sensor sits on which jack port, what was found on I²C,
 * and calibration offsets. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI, $ = T.$;

  const I2C = [
    ['bmx', 'BME280 / BMP280', 'temperature, humidity, pressure'],
    ['bme680', 'BME680', 'temperature, humidity, pressure, air quality'],
    ['aht20', 'AHT20', 'temperature, humidity'],
    ['am2320', 'AM2320', 'temperature, humidity'],
    ['max44009', 'MAX44009', 'ambient light'],
    ['sgp30', 'SGP30', 'TVOC, eCO₂'],
    ['ds3231', 'DS3231 clock', 'real-time clock for the data log, alarms'],
    ['eeprom', 'EEPROM', 'data-log memory']
  ];

  function i2cInner() {
    const d = T.State.info.detected;
    if (!d) {
      const keys = T.Readings.known();
      return html`<p class="hint">Found automatically at start-up — nothing to switch on. ${keys.length ? 'Readings present: ' + keys.filter(k => !/^cs/.test(k)).map(k => T.Readings.name(k)).join(', ') + '.' : 'Readings show on the dashboard once they arrive.'}</p>`;
    }
    // the 1 MB build leaves out the BME680, SGP30 and the clock/EEPROM module
    const supported = T.isGeneric() ? I2C.filter(s => ['bmx', 'aht20', 'am2320', 'max44009'].indexOf(s[0]) >= 0) : I2C;
    const found = supported.filter(s => d[s[0]]);
    return html`${found.length
      ? html`<ul class="mb0">${found.map(s => html`<li><strong>${s[1]}</strong> <span class="hint">${s[2]}</span></li>`)}</ul>`
      : html`<p class="hint mb0">Nothing found on the I²C bus at start-up. If a sensor is attached: check it sits in the port the right way round, and that no DHT/DS18B20 is enabled on Port B (green), which uses the same pins. The device scans again on every restart.</p>`}`;
  }

  T.definePage({
    id: 'sensors', title: 'Sensors',
    nav: { group: 'setup', icon: 'sliders', order: 1 },
    save: { reboot: true },
    render() {
      const c = T.State.config;
      const display = T.isDisplay();
      const generic = T.isGeneric();
      const portB = c.dht_sensor ? 'dht' : (c.ds18b20_sensor ? 'ds18b20' : 'none');
      const portA = c.second_dht_sensor ? 'dht' : (c.second_ds18b20_sensor ? 'ds18b20' : (c.adc_sensor ? 'adc' : 'none'));
      // first-generation boards have no second data pin: only the analog input is left on that side
      const portAOptions = [{ value: 'none', label: 'Nothing' }].concat(generic ? [] : [
        { value: 'dht', label: 'DHT11 / DHT21 / DHT22', hint: 'Temperature and humidity → %temp2% / %humi2%' },
        { value: 'ds18b20', label: 'DS18B20', hint: 'Temperature (waterproof probes) → %temp2%' }
      ], [{ value: 'adc', label: 'Analog sensor (ADC)', hint: 'Soil moisture, light, a voltage → %adc%' }]);
      return html`${UI.pagehead('Sensors', 'Tell the device what is plugged into its ports. I²C sensors are found by themselves.')}
        ${UI.note('warn', 'Only switch on a sensor that is really attached: the firmware waits for it, and a missing one can keep the device restarting.')}
        <div class="grid-2">
        ${UI.card({ title: generic ? 'Analog input' : 'Port A (black)', icon: 'cpu', body: html`
          <p class="hint">${generic ? 'The first-generation board has one sensor port (below) plus the analog input.' : 'Has its own pin, so it never gets in the way of I²C sensors or the clock/data-log module. One sensor at a time.'}</p>
          ${UI.choice({ name: 'portA', value: portA, options: portAOptions })}` })}
        ${display
          ? UI.card({ title: 'Port B (green)', icon: 'cpu', body: html`<p class="hint mb0">On the Display Weatherstation these pins drive the screen, so Port B is I²C only: plug I²C sensors and the clock module in here.</p>` })
          : UI.card({ title: 'Port B (green)', icon: 'cpu', body: html`
          <p class="hint">${generic ? 'Shares its pins with the I²C bus: while a DHT or DS18B20 is enabled here, I²C sensors are not available.' : 'Shares its pins with the I²C bus: while a DHT or DS18B20 is enabled here, I²C sensors and the clock/data-log module are not available. Prefer Port A for a DHT if you use the data log.'}</p>
          ${UI.choice({ name: 'portB', value: portB, options: [
            { value: 'none', label: 'Nothing / I²C sensors' },
            { value: 'dht', label: 'DHT11 / DHT21 / DHT22', hint: 'Temperature and humidity → %temp% / %humi%' },
            { value: 'ds18b20', label: 'DS18B20', hint: 'Temperature → %temp%' }
          ] })}` })}
        </div>
        ${UI.card({ title: 'I²C sensors & modules', icon: 'activity', body: html`<div id="i2c-found">${i2cInner()}</div>` })}
        ${UI.card({ title: 'Calibration', icon: 'sliders', body: html`
          <p class="hint">Offsets added to the readings. Compare with a reference — a good thermometer, a salt test for humidity (75 %RH over saturated NaCl), the local pressure at your altitude — and enter the difference.</p>
          ${UI.toggle({ id: 'calibrationActive', label: 'Apply the offsets', checked: !!c.calibrationActive })}
          <div class="grid-3 mt">
            ${UI.field({ id: 'calibrationTemp', label: 'Temperature', labelHint: '°C', type: 'number', value: c.calibrationTemp == null ? 0 : c.calibrationTemp, attrs: 'step="0.1"', hint: 'e.g. 1.5 if the sensor reads 22 when it is 23.5, or -1.5 the other way round' })}
            ${UI.field({ id: 'calibrationHumi', label: 'Humidity', labelHint: '%RH', type: 'number', value: c.calibrationHumi == null ? 0 : c.calibrationHumi, attrs: 'step="0.1"' })}
            ${UI.field({ id: 'calibrationQfe', label: 'Pressure', labelHint: 'hPa', type: 'number', value: c.calibrationQfe == null ? 0 : c.calibrationQfe, attrs: 'step="0.1"', hint: 'Station pressure (QFE), not sea-level (QNH)' })}
          </div>` })}`;
    },
    collect() {
      const display = T.isDisplay();
      const portA = T.radio('portA');
      const out = {
        second_dht_sensor: portA === 'dht',
        second_ds18b20_sensor: portA === 'ds18b20',
        adc_sensor: portA === 'adc',
        calibrationActive: T.checked('calibrationActive'),
        calibrationTemp: T.num('calibrationTemp', 0),
        calibrationHumi: T.num('calibrationHumi', 0),
        calibrationQfe: T.num('calibrationQfe', 0)
      };
      if (!display) {
        const portB = T.radio('portB');
        out.dht_sensor = portB === 'dht';
        out.ds18b20_sensor = portB === 'ds18b20';
      }
      return out;
    },
    on: {
      info() { T.render($('#i2c-found'), i2cInner()); },
      sensors() { if (!T.State.info.detected) T.render($('#i2c-found'), i2cInner()); }
    }
  });
})();
