<?php require __DIR__ . '/inc/cors.php'; ?>

<div class="d-flex justify-content-between flex-wrap flex-md-nowrap align-items-center pt-3 pb-2 mb-3 border-bottom">
    <h1 class="h2">System Settings</h1>
</div>

<div class="row">
    <div class="col-lg-6 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">System</div>
            <div class="card-body">
                <div class="alert alert-secondary small display-only d-none" role="alert">
                    <span data-feather="monitor"></span> The sleep modes below <strong>do not apply to the Display
                    Weatherstation</strong> — it is mains powered and its screen and clock have to keep running, so the
                    device ignores them. To run it without WiFi, use offline display mode (hold the right button for
                    10&nbsp;seconds, or see <a href="javascript:void(0);" onclick="ChangeContent(this, 'display_settings', '#right-content');">Display &amp; Alarms</a>).
                </div>
                <div class="display-not-applicable">
                <div class="form-check form-switch mb-2">
                    <input type="checkbox" class="form-check-input" id="sleepModeActive">
                    <label class="form-check-label" for="sleepModeActive">Deep Sleep (powersaving for battery operations)</label>
                    <small class="text-muted d-block">Device powers down between readings — lowest power, but unreachable and config mode is off while asleep.</small>
                </div>
                <div class="form-check form-switch mb-2">
                    <input type="checkbox" class="form-check-input" id="lightSleepModeActive">
                    <label class="form-check-label" for="lightSleepModeActive">Light Sleep (WiFi sleep only)</label>
                    <small class="text-muted d-block">CPU sleeps between readings; WiFi stays connected so the device stays reachable.</small>
                </div>
                </div>
                <div class="form-check form-switch mb-2">
                    <input type="checkbox" class="form-check-input" id="configModeActive" checked>
                    <label class="form-check-label" for="configModeActive">Config Mode Active (disable config mode to activate live mode)</label>
                    <small class="text-muted d-block display-hidden">This is the go-live switch: turn it off and Save (with Reboot ticked) and the device starts serving data. This web interface then goes away — to get back, press RESET then MODE within a second (RESET twice if the LED stays dark); blue LED = config mode.</small>
                    <small class="text-muted d-block display-only d-none">On the Display Weatherstation the <strong>screen, clock, alarms and this web interface keep running in every mode</strong> — this switch only controls data sending. Turn it off and Save (with Reboot ticked) to start sending to MQTT / HTTP / Home Assistant. Only the TEHYBUG setup access point goes away; reach this page at the device's normal IP address any time.</small>
                </div>
                <div class="form-check form-switch">
                    <input type="checkbox" class="form-check-input" id="reboot">
                    <label class="form-check-label" for="reboot">Reboot device after saving</label>
                    <small class="text-muted d-block">Restart the device on save so the changes take effect.</small>
                </div>
            </div>
        </div>
    </div>
    <div class="col-lg-6 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">Mode Information</div>
            <div class="card-body">
                <p><strong>Normal Mode (low frequency light sleep):</strong> WiFi is always on. Best for AC-powered devices requiring constant connectivity (e.g., BME680 air quality monitoring).</p>
                <p class="display-hidden"><strong>Light Sleep:</strong> WiFi sleeps between data transmissions. Good for balancing power saving and frequent updates.</p>
                <p class="display-hidden"><strong>Deep Sleep:</strong> The device almost completely powers down between transmissions. Ideal for long-term battery operation.</p>
                <p class="display-only d-none"><strong>Offline display mode:</strong> WiFi off, everything else running &mdash; the screen, clock, alarms and (if enabled) the data log carry on. The Display Weatherstation's alternative to the sleep modes.</p>
                <p><strong>Config Mode:</strong> The device stays active with WiFi on, allowing for configuration. Live data transmission is paused.</p>
            </div>
        </div>
    </div>
</div>

<div class="row">
    <div class="col-md-12 mb-4">
        <div class="alert alert-info" role="alert">
            <span data-feather="hard-drive"></span> Looking for the offline data log and EEPROM logging settings? They moved to their own
            <a href="javascript:void(0);" onclick="ChangeContent(this, 'datalog', '#right-content');">Data Log</a> page.
        </div>
    </div>
</div>

<?php require __DIR__ . '/inc/save_modal.php'; ?>


<!-- Power Consumption Guide -->
<div class="row mt-4">
    <div class="col-md-12">
        <div class="card border-warning">
            <div class="card-header bg-warning text-dark">
                <h4 class="mb-0"><span data-feather="battery-charging"></span> Power Consumption Guide</h4>
            </div>
            <div class="card-body">
                <div class="table-responsive">
                    <table class="table table-sm">
                        <thead>
                            <tr>
                                <th>Configuration</th>
                                <th>Power Usage</th>
                                <th>Battery Life (2000mAh)</th>
                                <th>Best For</th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr>
                                <td><strong>Normal Mode + 60s frequency</strong></td>
                                <td>~80mA continuous (WiFi always on)</td>
                                <td>~24 hours</td>
                                <td>BME680 air quality monitoring, AC powered</td>
                            </tr>
                            <tr>
                                <td><strong>Light Sleep + 300s frequency</strong></td>
                                <td>~5mA sleep + 100mA wake (5s every 5min)</td>
                                <td>~16 days</td>
                                <td>Regular monitoring, moderate battery life</td>
                            </tr>
                            <tr>
                                <td><strong>Deep Sleep + 900s frequency</strong></td>
                                <td>~20µA sleep + 100mA wake (5s every 15min)</td>
                                <td>~3-5 months</td>
                                <td>Long-term battery operation, periodic updates</td>
                            </tr>
                            <tr>
                                <td><strong>Deep Sleep + 3600s frequency</strong></td>
                                <td>~20µA sleep + 100mA wake (5s every hour)</td>
                                <td>~6-12 months</td>
                                <td>Maximum battery life, hourly updates</td>
                            </tr>
                        </tbody>
                    </table>
                </div>
                <div class="alert alert-info small mb-0">
                    <strong><span data-feather="zap"></span> Pro Tip:</strong> For battery operation, use Deep Sleep with 900s (15 min) or higher frequency. For AC power with air quality monitoring, use Normal Mode.
                </div>
            </div>
        </div>
    </div>
</div>

<script>
    feather.replace();
    connectionStart();

    $(function () {
        $("#sleepModeActive").change(function() {
            if(this.checked) {
                $('#lightSleepModeActive').prop('checked', false);
            }
        });
        $("#lightSleepModeActive").change(function() {
            if(this.checked) {
                $('#sleepModeActive').prop('checked', false);
            }
        });
    });
</script>