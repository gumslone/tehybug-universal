<?php require __DIR__ . '/inc/cors.php'; ?>
<div class="d-flex justify-content-between flex-wrap flex-md-nowrap align-items-center pt-3 pb-2 mb-3 border-bottom">
    <h1 class="h2">Sensor Settings</h1>
</div>

<div class="row">
    <div class="col-lg-4 mb-4">
        <div class="card">
            <div class="card-header" style="background-color: #34495e; color: white;">
                Sensors Port B (Green)
            </div>
            <div class="card-body">
                <div class="form-check form-switch">
                    <input type="checkbox" class="form-check-input" id="dht_sensor">
                    <label class="form-check-label" for="dht_sensor">DHTXX Active</label>
                    <small class="text-muted d-block">DHT11/21/22 temperature &amp; humidity sensor on Port B (green).</small>
                </div>
                <div class="form-check form-switch">
                    <input type="checkbox" class="form-check-input" id="ds18b20_sensor">
                    <label class="form-check-label" for="ds18b20_sensor">DS18B20 Active</label>
                    <small class="text-muted d-block">DS18B20 temperature sensor on Port B. Cannot be combined with DHT on the same port.</small>
                </div>
            </div>
        </div>
    </div>
    <div class="col-lg-4 mb-4">
        <div class="card">
            <div class="card-header" style="background-color: #34495e; color: white;">
                Sensors Port A (Black)
            </div>
            <div class="card-body">
                <div class="form-check form-switch">
                    <input type="checkbox" class="form-check-input" id="second_dht_sensor">
                    <label class="form-check-label" for="second_dht_sensor">DHTXX Active</label>
                    <small class="text-muted d-block">DHT sensor on Port A (black).</small>
                </div>
                <div class="form-check form-switch">
                    <input type="checkbox" class="form-check-input" id="second_ds18b20_sensor">
                    <label class="form-check-label" for="second_ds18b20_sensor">DS18B20 Active</label>
                    <small class="text-muted d-block">DS18B20 sensor on Port A.</small>
                </div>
                <div class="form-check form-switch">
                    <input type="checkbox" class="form-check-input" id="adc_sensor">
                    <label class="form-check-label" for="adc_sensor">ADC Active</label>
                    <small class="text-muted d-block">Read the analog input on Port A (e.g. soil moisture).</small>
                </div>
            </div>
        </div>
    </div>
    <div class="col-lg-4 mb-4">
        <div class="card">
            <div class="card-header" style="background-color: #34495e; color: white;">
                Calibration
            </div>
            <div class="card-body">
                <div class="form-group mb-3">
                    <label for="calibrationTemp">Temperature</label>
                    <input type="number" class="form-control" step=".1" id="calibrationTemp" value="0">
                    <small class="text-muted">Offset added to the temperature reading, in °C (e.g. -1.5).</small>
                </div>
                <div class="form-group mb-3">
                    <label for="calibrationHumi">Humidity</label>
                    <input type="number" class="form-control" step=".1" id="calibrationHumi" value="0">
                    <small class="text-muted">Offset added to the humidity reading, in %RH.</small>
                </div>
                <div class="form-group mb-3">
                    <label for="calibrationQfe">Barometric Air Pressure</label>
                    <input type="number" class="form-control" step="1" id="calibrationQfe" value="0">
                    <small class="text-muted">Offset added to the barometric pressure reading, in hPa.</small>
                </div>
                <div class="form-check form-switch">
                    <input type="checkbox" class="form-check-input" id="calibrationActive">
                    <label class="form-check-label" for="calibrationActive">Calibration active</label>
                    <small class="text-muted d-block">Apply the offsets above to the readings.</small>
                </div>
            </div>
        </div>
    </div>
</div>


<div class="col-md-12 mt-4">
    <div class="alert alert-danger text-center" role="alert">
        <strong>Warning:</strong> Activating a sensor that is not connected may cause the device to enter a reboot loop. Please ensure your selections match your physical hardware.
    </div>
</div>
<?php require __DIR__ . '/inc/save_modal.php'; ?>

<!-- Sensor Information Section -->
<div class="row mt-4">
    <div class="col-md-12">
        <div class="card border-info">
            <div class="card-header bg-info text-white">
                <h4 class="mb-0"><span data-feather="thermometer"></span> Sensor Configuration Guide</h4>
            </div>
            <div class="card-body">
                <div class="row">
                    <div class="col-lg-6 mb-3">
                        <h5><span data-feather="cpu"></span> Port B (Green Connector)</h5>
                        <ul class="small">
                            <li><strong>DHTXX:</strong> DHT11, DHT21, DHT22 temperature & humidity sensors</li>
                            <li><strong>DS18B20:</strong> Digital temperature sensor (waterproof available)</li>
                            <li><strong>Note:</strong> Only one sensor type can be active per port</li>
                        </ul>
                        <div class="alert alert-warning small mb-0">
                            <strong>Important:</strong> DHT and DS18B20 cannot be used simultaneously on the same port
                        </div>
                    </div>
                    
                    <div class="col-lg-6 mb-3">
                        <h5><span data-feather="cpu"></span> Port A (Black Connector)</h5>
                        <ul class="small">
                            <li><strong>DHTXX:</strong> Second DHT sensor for dual-zone monitoring</li>
                            <li><strong>DS18B20:</strong> Second temperature sensor</li>
                            <li><strong>ADC:</strong> Analog input (0-3.3V) for custom sensors</li>
                            <li><strong>Note:</strong> Only one sensor type can be active per port</li>
                        </ul>
                        <div class="alert alert-info small mb-0">
                            <strong>Tip:</strong> Use ADC for soil moisture, light sensors, or voltage monitoring
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>
</div>

<!-- Calibration Guide -->
<div class="row mt-4">
    <div class="col-md-12">
        <div class="card border-warning">
            <div class="card-header bg-warning text-dark">
                <h4 class="mb-0"><span data-feather="sliders"></span> Calibration Guide</h4>
            </div>
            <div class="card-body">
                <div class="row">
                    <div class="col-lg-4 mb-3">
                        <h5>Temperature Calibration</h5>
                        <ul class="small">
                            <li>Compare with a reference thermometer</li>
                            <li>Enter the difference (e.g., +1.5 or -0.8)</li>
                            <li>Positive value increases reading</li>
                            <li>Negative value decreases reading</li>
                        </ul>
                        <div class="alert alert-info small mb-0">
                            <strong>Example:</strong> If sensor shows 22°C but actual is 23.5°C, enter +1.5
                        </div>
                    </div>
                    
                    <div class="col-lg-4 mb-3">
                        <h5>Humidity Calibration</h5>
                        <ul class="small">
                            <li>Use salt test method for accuracy</li>
                            <li>Place sensor in sealed container with saturated salt solution</li>
                            <li>Wait 8-12 hours, should read 75%RH</li>
                            <li>Enter correction value</li>
                        </ul>
                        <div class="alert alert-info small mb-0">
                            <strong>Salt Test:</strong> NaCl solution should stabilize at 75%RH at 20°C
                        </div>
                    </div>
                    
                    <div class="col-lg-4 mb-3">
                        <h5>Pressure Calibration</h5>
                        <ul class="small">
                            <li>Compare with local weather station</li>
                            <li>Use QFE (station pressure) not QNH</li>
                            <li>Enter difference in hPa</li>
                            <li>Altitude affects pressure readings</li>
                        </ul>
                        <div class="alert alert-info small mb-0">
                            <strong>Note:</strong> Pressure drops ~12 hPa per 100m altitude increase
                        </div>
                    </div>
                </div>
                <div class="alert alert-success small mb-0">
                    <strong><span data-feather="check"></span> Remember:</strong> Enable "Calibration active" checkbox after entering values for corrections to take effect.
                </div>
            </div>
        </div>
    </div>
</div>


<script>
    feather.replace();
    connectionStart();   

    $(function () {

        $("#dht_sensor").change(function() {
            if(this.checked) {
                //$('#am2320_sensor').prop('checked', false);
                $('#ds18b20_sensor').prop('checked', false);
            }
        });
        $("#ds18b20_sensor").change(function() {
            if(this.checked) {
                $('#dht_sensor').prop('checked', false);
                //$('#am2320_sensor').prop('checked', false);
            }
        });
        $("#second_ds18b20_sensor").change(function() {
            if(this.checked) {
                $('#adc_sensor').prop('checked', false);
                $('#second_dht_sensor').prop('checked', false);
            }
        });
        $("#adc_sensor").change(function() {
            if(this.checked) {
                $('#second_ds18b20_sensor').prop('checked', false);
                $('#second_dht_sensor').prop('checked', false);
            }
        });
        $("#second_dht_sensor").change(function() {
            if(this.checked) {
                $('#adc_sensor').prop('checked', false);
                $('#second_ds18b20_sensor').prop('checked', false);
            }
        });

        }); 
</script>