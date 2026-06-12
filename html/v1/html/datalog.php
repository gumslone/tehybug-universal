<?php require __DIR__ . '/inc/cors.php'; ?>

<div class="d-flex justify-content-between flex-wrap flex-md-nowrap align-items-center pt-3 pb-2 mb-3 border-bottom">
    <h1 class="h2">Data Log</h1>
</div>

<div class="card mb-4">
    <div class="card-header" style="background-color: #34495e; color: white;">
        About the offline data log
    </div>
    <div class="card-body">
        <p class="mb-0">When a DS3231 RTC + EEPROM module is attached, the device can store
        timestamped readings locally, with no server or network involved. Entries are written
        to one file per day of month; the oldest day file is recycled once the EEPROM is full.
        Set the device clock once (below) so timestamps are meaningful.</p>
    </div>
</div>

<div class="row">
    <!-- EEPROM log configuration -->
    <div class="col-lg-6 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">EEPROM Log</div>
            <div class="card-body">
                <div class="form-check form-switch mb-3">
                    <input type="checkbox" class="form-check-input" id="eepromLogActive">
                    <label class="form-check-label" for="eepromLogActive">Log readings to EEPROM</label>
                </div>
                <div class="form-group mb-3">
                    <label for="eepromLogFrequency" class="form-label">Log Frequency (seconds)</label>
                    <input type="number" class="form-control" id="eepromLogFrequency" min="60" value="60">
                    <small class="text-muted">How often a line is written. In offline mode this is also the deep-sleep interval between wakeups. Timestamps have one-minute resolution, so 60 s or more is recommended.</small>
                </div>
                <div class="form-group mb-3">
                    <label for="eepromLogMessage" class="form-label">Logged Fields (placeholder template)</label>
                    <input type="text" class="form-control" id="eepromLogMessage" placeholder="leave empty for all measured fields">
                    <small class="text-muted">Choose exactly which values to store using placeholders, e.g. <code>%temp% %humi%</code> to log only temperature and humidity. Leave empty to log the default set of measured values. See the placeholder names on the <a href="javascript:void(0);" onclick="ChangeContent(this, 'settings', '#right-content');">Data Serving</a> page.</small>
                </div>
            </div>
        </div>
    </div>

    <!-- Offline mode -->
    <div class="col-lg-6 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">Offline Mode</div>
            <div class="card-body">
                <div class="form-check form-switch mb-3">
                    <input type="checkbox" class="form-check-input" id="offlineModeActive">
                    <label class="form-check-label" for="offlineModeActive">Offline mode (no WiFi, EEPROM only)</label>
                </div>
                <p class="small">In offline mode the device never connects to WiFi. It wakes on the log frequency, measures, appends one entry to the EEPROM log and deep-sleeps again &mdash; the lowest possible power draw and no network needed.</p>
                <div class="alert alert-warning small mb-0">
                    <strong><span data-feather="alert-triangle"></span> Reading the log:</strong> the web interface is unavailable while offline. To read the stored data, press <strong>RESET</strong>, then press and hold <strong>MODE</strong> until the LED turns blue to re-enter config mode (WiFi on), then come back to this page.
                </div>
                <div id="offline_unavailable" class="alert alert-secondary small mt-3 mb-0" style="display:none;">
                    <strong>Note:</strong> No RTC + EEPROM module was detected, so offline mode and logging have no effect on this device.
                </div>
            </div>
        </div>
    </div>
</div>

<?php require __DIR__ . '/inc/save_modal.php'; ?>

<!-- Stored data reader -->
<div class="row">
    <div class="col-md-12 mb-4">
        <div class="card">
            <div class="card-header" style="background-color: #34495e; color: white;">Stored Data</div>
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
                    <p class="small text-muted mb-2">One file per day of month is written; the oldest file is recycled when the EEPROM is full.</p>
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

<script>
    feather.replace();
    connectionStart();

    loadDataLog();

    function dataLogUrl(query) {
        return 'http://' + ipAddress + '/api/' + query;
    }

    function loadDataLog() {
        $.getJSON(dataLogUrl('datalog'), function (data) {
            if (!data.active) {
                $('#datalog_status').text('No RTC + EEPROM module detected.');
                $('#datalog_panel').hide();
                $('#offline_unavailable').show();
                return;
            }
            $('#offline_unavailable').hide();
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
