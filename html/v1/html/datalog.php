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
        Set the device clock once (below) so timestamps are meaningful. To save space each line
        stores only the time of day (the date is the file name) and tags each value with a short
        code &mdash; e.g. <code>07:55 22.6t 48.3h 1013.2p</code>.</p>
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
                    <strong><span data-feather="alert-triangle"></span> Reading the log:</strong> the web interface is unavailable while offline. To read the stored data, press and release <strong>RESET</strong> (do <strong>not</strong> hold MODE during reset &mdash; that boots the ESP into flash mode), then within a few seconds press and hold <strong>MODE</strong> until the LED turns <strong>blue</strong> to re-enter config mode (WiFi on), and come back to this page.
                </div>
                <div id="offline_exclusive" class="alert alert-info small mt-3 mb-0" style="display:none;">
                    <strong><span data-feather="info"></span> Offline mode is exclusive:</strong> saving it switches off every other mode (MQTT, Home Assistant, HTTP GET/POST, deep/light sleep and config mode) and turns EEPROM logging on.
                </div>
                <div id="offline_unavailable" class="alert alert-secondary small mt-3 mb-0" style="display:none;">
                    <strong>Note:</strong> No RTC + EEPROM module was detected, so offline mode and logging have no effect on this device.
                </div>
            </div>
        </div>
    </div>

    <!-- Other modes are saved with this form. They mirror the current device
         config (populated over the websocket) so a plain save leaves them
         untouched; enabling offline mode above switches them all off via JS.
         reboot is forced so EEPROM-log / offline-mode changes take effect. -->
    <div style="display:none;">
        <input type="checkbox" class="form-check-input" id="mqttActive">
        <input type="checkbox" class="form-check-input" id="haActive">
        <input type="checkbox" class="form-check-input" id="httpGetActive">
        <input type="checkbox" class="form-check-input" id="httpPostActive">
        <input type="checkbox" class="form-check-input" id="sleepModeActive">
        <input type="checkbox" class="form-check-input" id="lightSleepModeActive">
        <input type="checkbox" class="form-check-input" id="configModeActive">
        <input type="checkbox" class="form-check-input dont-change" id="reboot" checked>
    </div>
</div>

<!-- Storage capacity & limits -->
<div class="row">
    <div class="col-md-12 mb-4">
        <div class="card border-warning">
            <div class="card-header bg-warning text-dark">
                <h4 class="mb-0"><span data-feather="hard-drive"></span> Storage capacity &amp; limits</h4>
            </div>
            <div class="card-body">
                <p>The log lives on the 32&nbsp;KB I&#178;C EEPROM (FT24C256A) of the RTC module,
                which is divided into <strong>32 slots &mdash; one file per day of month</strong>.
                Each slot holds roughly <strong>1&nbsp;KB (about 1006 bytes)</strong>, so a full
                month of daily files fits.</p>
                <ul class="small">
                    <li><strong>Up to a full month is kept</strong> (one file per day of month). When all slots are in use and a new day starts, the oldest day file is recycled (erased) to make room.</li>
                    <li><strong>A day file that fills up stops accepting entries</strong> for the rest of that day; logging resumes in the next day's file. Pick a frequency so a full day fits.</li>
                    <li><strong>Fewer fields &amp; a longer frequency = more coverage.</strong> Use the <em>Logged Fields</em> template above to store only what you need.</li>
                </ul>
                <p class="small mb-2">Rough capacity per ~1&nbsp;KB day file with the compact format:</p>
                <div class="table-responsive">
                    <table class="table table-sm">
                        <thead>
                            <tr>
                                <th>Logged fields</th>
                                <th>~Bytes per entry</th>
                                <th>Entries per day file</th>
                                <th>Frequency to cover a full 24&nbsp;h</th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr><td>1 (e.g. <code>%temp%</code>)</td><td>~13</td><td>~77</td><td>~20&nbsp;min (1200&nbsp;s)</td></tr>
                            <tr><td>2 (e.g. <code>%temp% %humi%</code>)</td><td>~19</td><td>~52</td><td>~28&nbsp;min (1680&nbsp;s)</td></tr>
                            <tr><td>3 (default-style)</td><td>~25</td><td>~40</td><td>~36&nbsp;min (2160&nbsp;s)</td></tr>
                            <tr><td>4</td><td>~31</td><td>~32</td><td>~45&nbsp;min (2700&nbsp;s)</td></tr>
                        </tbody>
                    </table>
                </div>
                <div class="alert alert-info small mb-0">
                    <strong><span data-feather="info"></span> Example:</strong> logging temperature + humidity
                    every 30&nbsp;minutes stores ~48 readings per day &mdash; about one day file, so a full
                    month of days stays available. Logging every minute fills a day file in well under an
                    hour, after which the rest of that day is not recorded.
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
                    <details class="small text-muted mb-2">
                        <summary>Field code legend (default logging)</summary>
                        <div class="mt-1">
                            <code>t</code> temperature &middot; <code>t2</code> temperature 2 &middot;
                            <code>h</code> humidity &middot; <code>h2</code> humidity 2 &middot;
                            <code>p</code> pressure (hPa) &middot; <code>al</code> altitude (m) &middot;
                            <code>l</code> light (lux) &middot; <code>x</code> ADC &middot;
                            <code>q</code> IAQ &middot; <code>c</code> eCO&#8322; &middot;
                            <code>v</code> bVOC &middot; <code>a</code> gas resistance (kOhm)
                        </div>
                        <div class="mt-1">A custom logging template stores your own text instead.</div>
                    </details>
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

    $(function () {
        // Offline mode is exclusive. When it is enabled, switch off every
        // other serving / sleep / config mode and make sure logging is on,
        // so saving the form lands the device in a clean offline state.
        $("#offlineModeActive").change(function () {
            $("#offline_exclusive").toggle(this.checked);
            if (this.checked) {
                $("#mqttActive, #haActive, #httpGetActive, #httpPostActive, " +
                  "#sleepModeActive, #lightSleepModeActive, #configModeActive")
                    .prop('checked', false);
                $("#eepromLogActive").prop('checked', true);
            }
        });
    });

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
