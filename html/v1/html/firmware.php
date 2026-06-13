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
                        <td>TeHyBug universal &amp; Mini TeHyBug (ESP8285) <span class="badge bg-success">recommended</span></td>
                        <td><code>tehybug.ino.esp8285.bin</code></td>
                        <td><a href="https://github.com/gumslone/tehybug-universal/raw/main/firmware/tehybug.ino.esp8285.bin" target="_blank">Download</a></td>
                    </tr>
                    <tr>
                        <td>ESP8285 (universal / Mini) with serial debug output</td>
                        <td><code>tehybug.ino.esp8285_debug.bin</code></td>
                        <td><a href="https://github.com/gumslone/tehybug-universal/raw/main/firmware/tehybug.ino.esp8285_debug.bin" target="_blank">Download</a></td>
                    </tr>
                    <tr>
                        <td>Old / first-gen TeHyBug (esp-01 based, generic ESP8266, 1&nbsp;MB)</td>
                        <td><code>tehybug.ino.generic.bin</code></td>
                        <td><a href="https://github.com/gumslone/tehybug-universal/raw/main/firmware/tehybug.ino.generic.bin" target="_blank">Download</a></td>
                    </tr>
                </tbody>
            </table>
        </div>
        <div class="alert alert-info small mb-0">
            <strong>Which file?</strong> Use the <code>esp8285</code> build for the TeHyBug universal <em>and</em> Mini
            TeHyBug (both use the ESP8285). The <code>generic</code> build is only for old / first-generation TeHyBugs
            (esp-01 based generic ESP8266). The <code>_debug</code> build is only for troubleshooting (it prints over
            serial and is larger).
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
        <p class="small text-muted">Loaded from the project repository, so it is always current.</p>
        <div id="changelog" class="small">
            <p class="text-muted">Loading changelog&hellip;</p>
        </div>
    </div>
</div>

<script>
    feather.replace();
    connectionStart();

    // Pull the changelog from the repository so it stays current without
    // re-deploying this page. GitHub raw sends a permissive CORS header.
    (function () {
        var target = document.getElementById('changelog');
        if (!target) { return; }
        var url = 'https://raw.githubusercontent.com/gumslone/tehybug-universal/main/CHANGELOG.md';

        function esc(s) { return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }
        function inline(s) {
            return esc(s)
                .replace(/`([^`]+)`/g, '<code>$1</code>')
                .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
                .replace(/\[([^\]]+)\]\((https?:[^)]+)\)/g, '<a href="$2" target="_blank">$1</a>');
        }
        function mdToHtml(md) {
            var lines = md.split('\n'), out = [], inList = false, para = [], m;
            function closeList() { if (inList) { out.push('</ul>'); inList = false; } }
            function flushPara() { if (para.length) { out.push('<p>' + para.join(' ') + '</p>'); para = []; } }
            for (var i = 0; i < lines.length; i++) {
                var line = lines[i];
                if ((m = line.match(/^###\s+(.*)/)))      { flushPara(); closeList(); out.push('<h6 class="mt-2">' + inline(m[1]) + '</h6>'); }
                else if ((m = line.match(/^##\s+(.*)/)))   { flushPara(); closeList(); out.push('<h6 class="mt-3">' + inline(m[1]) + '</h6>'); }
                else if (line.match(/^#\s+/))              { flushPara(); closeList(); /* top-level title: card header already says Changelog */ }
                else if ((m = line.match(/^\s*[-*]\s+(.*)/))) { flushPara(); if (!inList) { out.push('<ul>'); inList = true; } out.push('<li>' + inline(m[1]) + '</li>'); }
                else if (line.trim() === '')               { flushPara(); closeList(); }
                else                                       { para.push(inline(line)); }
            }
            flushPara();
            closeList();
            return out.join('\n');
        }

        fetch(url, { cache: 'no-store' })
            .then(function (r) { if (!r.ok) { throw new Error(r.status); } return r.text(); })
            .then(function (md) {
                target.innerHTML = mdToHtml(md);
                if (typeof feather !== 'undefined') { feather.replace(); }
            })
            .catch(function () {
                target.innerHTML = '<div class="alert alert-secondary mb-0">Could not load the changelog. ' +
                    'Read it on <a href="https://github.com/gumslone/tehybug-universal/blob/main/CHANGELOG.md" target="_blank">GitHub</a>.</div>';
            });
    })();
</script>
