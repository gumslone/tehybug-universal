<?php require __DIR__ . '/inc/cors.php'; ?>

<div class="d-flex justify-content-between flex-wrap flex-md-nowrap align-items-center pt-3 pb-2 mb-3 border-bottom">
    <h1 class="h2">Display &amp; Alarms</h1>
</div>

<div class="row">
    <div class="col-md-12 mb-3">
        <div class="alert alert-info" role="alert">
            <span data-feather="monitor"></span> These settings apply to the <strong>TeHyBug Display Weatherstation</strong>
            (1.3&Prime; OLED, real-time clock, buzzer). Other TeHyBug boards ignore them.
        </div>
    </div>
</div>

<!-- Display pages -->
<div class="row">
    <div class="col-lg-6 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">Display Pages</div>
            <div class="card-body">
                <p class="small text-muted">
                    The display has two pages, switched with the <strong>left / right buttons</strong>:
                    a <strong>clock page</strong> (date, big time, and lines 1&nbsp;+&nbsp;2 as a footer)
                    and a <strong>sensor page</strong> showing all three lines. Lines are templates:
                    placeholders like <code>%temp%</code> are replaced with live readings.
                </p>
                <div class="mb-2">
                    <label for="line1" class="form-label">Line 1 <small class="text-muted">(sensor page + clock footer)</small></label>
                    <input type="text" class="form-control" id="line1" placeholder="%temp% °C">
                </div>
                <div class="mb-2">
                    <label for="line2" class="form-label">Line 2 <small class="text-muted">(sensor page + clock footer)</small></label>
                    <input type="text" class="form-control" id="line2" placeholder="%humi% %RH">
                </div>
                <div class="mb-2">
                    <label for="line3" class="form-label">Line 3 <small class="text-muted">(sensor page only)</small></label>
                    <input type="text" class="form-control" id="line3" placeholder="%qfe% hPa">
                </div>
                <small class="text-muted d-block mt-2">
                    Each line fits about 18 characters in the sensor-page font.
                    A placeholder with no matching sensor is shown as written &mdash; that is how you spot a typo.
                </small>
            </div>
        </div>
    </div>
    <div class="col-lg-6 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">Placeholders this device provides</div>
            <div class="card-body">
                <p class="small text-muted mb-2">Live list from the sensors detected on this device. Wait a moment after opening the page for readings to arrive.</p>
                <div class="table-responsive" style="max-height: 260px; overflow-y: auto;">
                    <table class="table table-sm table-striped">
                        <thead><tr><th>Reading</th><th>Placeholder</th><th>Unit</th></tr></thead>
                        <tbody id="sensor_data"></tbody>
                    </table>
                </div>
            </div>
        </div>
    </div>
</div>

<!-- Clock -->
<div class="row">
    <div class="col-lg-6 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">Clock</div>
            <div class="card-body">
                <div class="form-check form-switch mb-2">
                    <input type="checkbox" class="form-check-input" id="clock_12h">
                    <label class="form-check-label" for="clock_12h">12-hour clock</label>
                    <small class="text-muted d-block">Show the clock page as 1&ndash;12 with an am/pm marker instead of 0&ndash;23.</small>
                </div>
                <div class="form-check form-switch mb-3">
                    <input type="checkbox" class="form-check-input" id="clock_show_ip" checked>
                    <label class="form-check-label" for="clock_show_ip">Show IP address on the clock page</label>
                    <small class="text-muted d-block">Printed in tiny type along the right edge, so you can always find the device on your network. When WiFi is switched off the edge shows &ldquo;wifi off&rdquo; instead.</small>
                </div>
                <hr>
                <p class="mb-1"><strong>Device clock:</strong> <span id="display_clock_now" class="text-muted">&mdash;</span></p>
                <button type="button" class="btn btn-outline-primary btn-sm" onclick="setDisplayClock()">
                    <span data-feather="clock"></span> Set clock from this browser
                </button>
                <small class="text-muted d-block mt-2">
                    The DS3231 clock chip keeps time on its backup battery once set.
                    Alarms and the offline data log only run after the clock has been set.
                </small>
            </div>
        </div>
    </div>
    <div class="col-lg-6 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">Night Mode</div>
            <div class="card-body">
                <div class="form-check form-switch mb-2">
                    <input type="checkbox" class="form-check-input" id="clock_sleep">
                    <label class="form-check-label" for="clock_sleep">Switch the display off at night</label>
                    <small class="text-muted d-block">The panel goes dark inside the window below. Everything else keeps running: alarms still ring (and light the panel while ringing), buttons still work, data is still served.</small>
                </div>
                <div class="row">
                    <div class="col-6 mb-2">
                        <label for="clock_sleep_start" class="form-label">Off from</label>
                        <input type="time" class="form-control" id="clock_sleep_start">
                    </div>
                    <div class="col-6 mb-2">
                        <label for="clock_sleep_finish" class="form-label">On again at</label>
                        <input type="time" class="form-control" id="clock_sleep_finish">
                    </div>
                </div>
                <small class="text-muted d-block">A window across midnight (e.g. 22:00 &rarr; 07:00) works as expected.</small>
            </div>
        </div>
    </div>
</div>

<!-- Alarms -->
<div class="row">
    <div class="col-md-12 mb-2">
        <h4>Alarms</h4>
        <p class="small text-muted">
            Up to three weekday-scheduled alarms. When one fires the buzzer alternates two tones once a second
            and the display shows the alarm message (if one is set). <strong>Press any display button to mute.</strong>
            Alarms need the device clock to be set.
        </p>
    </div>
<?php
$weekdayNames = array('Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun');
for ($i = 1; $i <= 3; $i++) {
?>
    <div class="col-lg-4 mb-4">
        <div class="card h-100">
            <div class="card-header" style="background-color: #34495e; color: white;">Alarm <?php echo $i; ?></div>
            <div class="card-body">
                <div class="form-check form-switch mb-2">
                    <input type="checkbox" class="form-check-input" id="alarm<?php echo $i; ?>Active">
                    <label class="form-check-label" for="alarm<?php echo $i; ?>Active">Enabled</label>
                </div>
                <div class="mb-2">
                    <label for="alarm<?php echo $i; ?>Time" class="form-label">Time</label>
                    <input type="time" class="form-control" id="alarm<?php echo $i; ?>Time">
                </div>
                <div class="mb-2">
                    <label for="alarm<?php echo $i; ?>Message" class="form-label">Message <small class="text-muted">(shown on the display)</small></label>
                    <input type="text" class="form-control" id="alarm<?php echo $i; ?>Message">
                </div>
                <label class="form-label">Weekdays</label>
                <div class="btn-group d-flex flex-wrap" role="group">
<?php foreach ($weekdayNames as $d => $name) { ?>
                    <input type="checkbox" class="btn-check alarm-weekday" data-alarm="<?php echo $i; ?>" data-day="<?php echo $d; ?>" id="alarm<?php echo $i; ?>_wd_<?php echo $d; ?>" autocomplete="off">
                    <label class="btn btn-outline-primary btn-sm" for="alarm<?php echo $i; ?>_wd_<?php echo $d; ?>"><?php echo $name; ?></label>
<?php } ?>
                </div>
                <!-- CSV the firmware stores: 7 flags, Monday..Sunday -->
                <input type="hidden" id="alarm<?php echo $i; ?>Weekdays" value="0,0,0,0,0,0,0">
            </div>
        </div>
    </div>
<?php } ?>
</div>

<!-- Hardware cheat sheet -->
<div class="row">
    <div class="col-md-12 mb-4">
        <div class="card border-info">
            <div class="card-header bg-info text-dark"><span data-feather="cpu"></span> Buttons on the device</div>
            <div class="card-body">
                <div class="table-responsive">
                    <table class="table table-sm mb-2">
                        <thead><tr><th>Button</th><th>Press</th><th>What it does</th></tr></thead>
                        <tbody>
                            <tr><td><strong>Left / Right</strong> (beside the display)</td><td>click</td><td>Switch between the clock and sensor pages &mdash; or mute a ringing alarm.</td></tr>
                            <tr><td><strong>Right (IO_5)</strong></td><td>hold 10&nbsp;s</td><td>Toggle offline display mode: WiFi off, clock and sensors keep running. The LED turns purple and the device restarts. Same hold switches WiFi back on.</td></tr>
                            <tr><td><strong>MODE</strong> (top)</td><td>press <em>after</em> RESET</td><td>Enter config mode (this web interface); the LED turns blue. Press it after the device boots &mdash; holding it <em>during</em> reset puts the chip in firmware-flash mode instead.</td></tr>
                            <tr><td><strong>MODE</strong> (top)</td><td>hold 20&nbsp;s</td><td>Factory reset: settings, WiFi credentials and the data log are erased. The LED turns red.</td></tr>
                            <tr><td><strong>RESET</strong></td><td>click</td><td>Reboot the device.</td></tr>
                        </tbody>
                    </table>
                </div>
                <small class="text-muted">Offline display mode is remembered across reboots. To get back into this web interface from it: press RESET, then hold MODE until the LED turns blue.</small>
            </div>
        </div>
    </div>
</div>

<?php require __DIR__ . '/inc/save_modal.php'; ?>

<script>
    feather.replace();
    connectionStart();

    // Weekday pills <-> the hidden "0,0,0,0,0,0,0" CSV (Monday..Sunday) the
    // firmware stores. The device pushes the CSV into the hidden inputs via
    // RefreshData, which sets values without firing events - so a short
    // interval mirrors hidden -> pills whenever the value changed.
    function weekdaysToPills() {
        for (var a = 1; a <= 3; a++) {
            var csv = ($('#alarm' + a + 'Weekdays').val() || '').split(',');
            for (var d = 0; d < 7; d++) {
                $('#alarm' + a + '_wd_' + d).prop('checked', csv[d] === '1');
            }
        }
    }
    function pillsToWeekdays(alarm) {
        var flags = [];
        for (var d = 0; d < 7; d++) {
            flags.push($('#alarm' + alarm + '_wd_' + d).prop('checked') ? '1' : '0');
        }
        $('#alarm' + alarm + 'Weekdays').val(flags.join(','));
    }
    $('.alarm-weekday').change(function () {
        pillsToWeekdays($(this).data('alarm'));
    });
    var lastWeekdayCsv = '';
    var weekdaySync = setInterval(function () {
        // The page lives in #right-content and is replaced wholesale when you
        // navigate; stop then, or every visit leaves another timer running.
        if (!$('#alarm1Weekdays').length) {
            clearInterval(weekdaySync);
            return;
        }
        var csv = $('#alarm1Weekdays').val() + '|' + $('#alarm2Weekdays').val() + '|' + $('#alarm3Weekdays').val();
        if (csv !== lastWeekdayCsv) {
            lastWeekdayCsv = csv;
            weekdaysToPills();
        }
    }, 500);

    // Device clock: show it and set it from the browser (same API the Data
    // Log page uses; wd is 1=Sunday..7=Saturday, the DS3231 convention).
    function loadDisplayClock() {
        $.getJSON('http://' + ipAddress + '/api/time', function (data) {
            if (!data.rtc) {
                $('#display_clock_now').text('no clock chip detected');
                return;
            }
            $('#display_clock_now').text(data.timeSet ? data.time : 'not set');
        });
    }
    function setDisplayClock() {
        var now = new Date();
        var query = 'settime?y=' + now.getFullYear() +
            '&mo=' + (now.getMonth() + 1) +
            '&d=' + now.getDate() +
            '&wd=' + (now.getDay() + 1) +
            '&h=' + now.getHours() +
            '&mi=' + now.getMinutes() +
            '&s=' + now.getSeconds();
        $.get('http://' + ipAddress + '/api/' + query, function () {
            loadDisplayClock();
        });
    }
    loadDisplayClock();
</script>
