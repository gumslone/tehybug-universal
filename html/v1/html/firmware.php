<?php require __DIR__ . '/inc/cors.php'; ?>

<div class="d-flex justify-content-between flex-wrap flex-md-nowrap align-items-center pt-3 pb-2 mb-3 border-bottom">
    <h1 class="h2">Firmware Downloads &amp; Changelog</h1>
</div>

<!-- Update advice -->
<div class="alert alert-warning" role="alert">
    <h5 class="mb-1"><span data-feather="alert-triangle"></span> Update only if you need to</h5>
    <p class="mb-0">If your TeHyBug is working the way you want, <strong>you do not need to update</strong>.
    Every firmware update can introduce new bugs, and an update may break a feature that currently works
    fine for you. Only update if you are missing a feature or fixing a problem you actually have &mdash;
    and note your current version (shown on the <a href="javascript:void(0);" onclick="ChangeContent(this, 'main', '#right-content');">Dashboard</a>) first so you can roll back if needed.</p>
</div>

<!-- Downloads -->
<div class="card mb-4">
    <div class="card-header" style="background-color: #34495e; color: white;">
        <span data-feather="download"></span> Download firmware
    </div>
    <div class="card-body">
        <p>Releases are published on GitHub. The latest release always has the newest binaries attached:</p>
        <p>
            <a class="btn btn-success" href="https://github.com/gumslone/tehybug-universal/releases/latest" target="_blank">
                <span data-feather="external-link"></span> Latest release
            </a>
            <a class="btn btn-outline-secondary ms-2" href="https://github.com/gumslone/tehybug-universal/releases" target="_blank">
                All releases
            </a>
        </p>
        <div class="table-responsive">
            <table class="table table-sm">
                <thead>
                    <tr><th>Board</th><th>File</th><th></th></tr>
                </thead>
                <tbody>
                    <tr>
                        <td>TeHyBug universal (ESP8285) <span class="badge bg-success">recommended</span></td>
                        <td><code>tehybug.ino.esp8285.bin</code></td>
                        <td><a href="https://github.com/gumslone/tehybug-universal/raw/main/firmware/tehybug.ino.esp8285.bin" target="_blank">Download</a></td>
                    </tr>
                    <tr>
                        <td>ESP8285 with serial debug output</td>
                        <td><code>tehybug.ino.esp8285_debug.bin</code></td>
                        <td><a href="https://github.com/gumslone/tehybug-universal/raw/main/firmware/tehybug.ino.esp8285_debug.bin" target="_blank">Download</a></td>
                    </tr>
                    <tr>
                        <td>Mini TeHyBug / generic ESP8266 (1&nbsp;MB)</td>
                        <td><code>tehybug.ino.generic.bin</code></td>
                        <td><a href="https://github.com/gumslone/tehybug-universal/raw/main/firmware/tehybug.ino.generic.bin" target="_blank">Download</a></td>
                    </tr>
                </tbody>
            </table>
        </div>
        <div class="alert alert-info small mb-0">
            <strong>Which file?</strong> Use the <code>esp8285</code> build for the TeHyBug universal boards and the
            <code>generic</code> build for the Mini TeHyBug. The <code>_debug</code> build is only for troubleshooting
            (it prints over serial and is larger).
        </div>
    </div>
</div>

<!-- How to update -->
<div class="card mb-4">
    <div class="card-header" style="background-color: #34495e; color: white;">
        <span data-feather="upload"></span> How to update
    </div>
    <div class="card-body">
        <ol class="mb-2">
            <li>Download the matching <code>.bin</code> for your board above.</li>
            <li>Open the <a href="javascript:void(0);" onclick="ChangeContentIframe(this, 'update', '#right-content');">Firmware Update</a> page (OTA), choose the file and upload it.</li>
            <li>The device reboots into the new firmware. Your configuration is kept.</li>
        </ol>
        <div class="alert alert-secondary small mb-0">
            Your current firmware version is shown on the
            <a href="javascript:void(0);" onclick="ChangeContent(this, 'main', '#right-content');">Dashboard</a>
            (System Info &rarr; Version).
        </div>
    </div>
</div>

<!-- Changelog -->
<div class="card mb-4">
    <div class="card-header" style="background-color: #34495e; color: white;">
        <span data-feather="list"></span> Changelog
    </div>
    <div class="card-body">
        <p class="small text-muted">Newest changes first. Firmware versions are date-based
        (<code>YYMMDDHHMM</code>); see the <a href="https://github.com/gumslone/tehybug-universal/releases" target="_blank">releases</a> for exact version tags.</p>

        <h6>Offline data logging</h6>
        <ul class="small">
            <li>Log readings to an attached RTC + EEPROM module with no server or network &mdash; one file per day of month, a full month retained.</li>
            <li>Pick exactly which values to log with placeholders (e.g. <code>%temp% %humi%</code>); compact on-device format fits more entries, and each day file keeps its full calendar date.</li>
            <li>New <strong>Offline mode</strong>: the device runs with WiFi off for the lowest power draw. Enabling it switches every other mode off.</li>
            <li>Configure and read the log on the <a href="javascript:void(0);" onclick="ChangeContent(this, 'datalog', '#right-content');">Data Log</a> page.</li>
        </ul>

        <h6>Usability</h6>
        <ul class="small">
            <li>Inline help under every setting on the configuration pages.</li>
            <li>More reliable return to config mode from offline / deep-sleep modes after a reset.</li>
        </ul>

        <h6>Fixes</h6>
        <ul class="small">
            <li>Offline mode no longer falls back to WiFi when the EEPROM is present.</li>
            <li>A day file reused in a new month no longer mixes dates.</li>
            <li>The dashboard sensor table is no longer cleared right after connecting.</li>
        </ul>
    </div>
</div>

<script>
    feather.replace();
    connectionStart();
</script>
