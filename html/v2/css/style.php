<?php
// The web UI's stylesheet, bundled from the files listed in ../manifest.json.
require __DIR__ . '/../inc/bundle.php';
serveBundle('css', 'text/css; charset=utf-8', "\n");
