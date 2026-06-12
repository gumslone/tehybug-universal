<?php require __DIR__ . '/inc/cors.php'; ?>

<div class="d-flex justify-content-between flex-wrap flex-md-nowrap align-items-center pt-3 pb-2 mb-3 border-bottom">
    <h1 class="h2">System Settings</h1>
</div>

<div class="row">
    <div class="col-lg-6 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">System</div>
            <div class="card-body">
                <div class="form-check form-switch mb-2">
                    <input type="checkbox" class="form-check-input" id="sleepModeActive">
                    <label class="form-check-label" for="sleepModeActive">Deep Sleep (powersaving for battery operations)</label>
                </div>
                <div class="form-check form-switch mb-2">
                    <input type="checkbox" class="form-check-input" id="lightSleepModeActive">
                    <label class="form-check-label" for="lightSleepModeActive">Light Sleep (WiFi sleep only)</label>
                </div>
                <div class="form-check form-switch mb-2">
                    <input type="checkbox" class="form-check-input" id="configModeActive" checked>
                    <label class="form-check-label" for="configModeActive">Config Mode Active (disable config mode to activate live mode)</label>
                </div>
                <div class="form-check form-switch">
                    <input type="checkbox" class="form-check-input" id="reboot">
                    <label class="form-check-label" for="reboot">Reboot device after saving</label>
                </div>
            </div>
        </div>
    </div>
    <div class="col-lg-6 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">Mode Information</div>
            <div class="card-body">
                <p><strong>Normal Mode (low frequency light sleep):</strong> WiFi is always on. Best for AC-powered devices requiring constant connectivity (e.g., BME680 air quality monitoring).</p>
                <p><strong>Light Sleep:</strong> WiFi sleeps between data transmissions. Good for balancing power saving and frequent updates.</p>
                <p><strong>Deep Sleep:</strong> The device almost completely powers down between transmissions. Ideal for long-term battery operation.</p>
                <p><strong>Config Mode:</strong> The device stays active with WiFi on, allowing for configuration. Live data transmission is paused.</p>
            </div>
        </div>
    </div>
</div>

<div class="row">
    <div class="col-md-12 mb-4">
        <div class="card">
            <div class="card-header" style="background-color: #34495e; color: white;">Offline Data Log (RTC + EEPROM module)</div>
            <div class="card-body">
                <p id="datalog_status" class="mb-2">Checking for RTC + EEPROM module...</p>
                <div id="datalog_panel" style="display: none;">
                    <p class="mb-2">
                        Device clock: <strong id="datalog_time">-</strong>
                        <button type="button" class="btn btn-sm btn-outline-primary ms-2" onclick="setDeviceClock()">
                            <span data-feather="clock"></span> Set clock from browser
                        </button>
                        <button type="button" class="btn btn-sm btn-outline-secondary ms-2" onclick="loadDataLog()">
                            <span data-feather="refresh-cw"></span> Refresh
                        </button>
                    </p>
                    <p class="small text-muted mb-2">In live mode the device appends one timestamped entry per minute to a file per day of month. The oldest file is recycled when the EEPROM is full.</p>
                    <div class="table-responsive">
                        <table class="table table-sm">
                            <thead><tr><th>File</th><th>Size</th><th></th></tr></thead>
                            <tbody id="datalog_files"></tbody>
                        </table>
                    </div>
                    <pre id="datalog_content" class="border rounded p-2 bg-light small" style="max-height: 300px; overflow: auto; display: none;"></pre>
                </div>
            </div>
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
        loadDataLog();
    });

    function dataLogUrl(query) {
        return 'http://' + ipAddress + '/api/' + query;
    }

    function loadDataLog() {
        $.getJSON(dataLogUrl('datalog'), function (data) {
            if (!data.active) {
                $('#datalog_status').text('No RTC + EEPROM module detected.');
                $('#datalog_panel').hide();
                return;
            }
            $('#datalog_status').hide();
            $('#datalog_panel').show();
            $('#datalog_time').text(data.timeSet ? data.time : 'not set (set it to start logging!)');

            const rows = $('#datalog_files');
            rows.html('');
            $.each(data.files, function (i, file) {
                rows.append('<tr><td>Day ' + file.name.replace('.txt', '') + ' (' + file.name + ')</td>' +
                    '<td>' + file.size + ' B</td>' +
                    '<td><button type="button" class="btn btn-sm btn-outline-primary" onclick="viewLogFile(\'' + file.name + '\')">View</button></td></tr>');
            });
            if (data.files.length === 0) {
                rows.append('<tr><td colspan="3">No log files yet.</td></tr>');
            }
            feather.replace();
        }).fail(function () {
            $('#datalog_status').text('Could not reach the device data log API.');
        });
    }

    function setDeviceClock() {
        const now = new Date();
        const query = 'settime?y=' + now.getFullYear() +
            '&mo=' + (now.getMonth() + 1) +
            '&d=' + now.getDate() +
            '&wd=' + (now.getDay() + 1) +
            '&h=' + now.getHours() +
            '&mi=' + now.getMinutes() +
            '&s=' + now.getSeconds();
        $.get(dataLogUrl(query), function () {
            loadDataLog();
        });
    }

    function viewLogFile(name) {
        $.get(dataLogUrl('datalog?file=' + encodeURIComponent(name)), function (content) {
            $('#datalog_content').text(content === '' ? '(empty file)' : content).show();
        });
    }
</script>