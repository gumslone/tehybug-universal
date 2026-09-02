<?php
// The web UI's script, bundled from the files listed in ../manifest.json.
require __DIR__ . '/../inc/bundle.php';
serveBundle('js', 'text/javascript; charset=utf-8', "\n;\n");
