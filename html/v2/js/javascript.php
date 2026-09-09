<?php
// The web UI's script, bundled from the files listed in ../manifest.json.
require __DIR__ . '/../inc/bundle.php';
// Wrapped so a second copy of the bundle (the online one arriving after the
// device's built-in fallback copy) does not run again - same as embed-ui.py.
serveBundle('js', 'text/javascript; charset=utf-8', "\n;\n", "if (!window.TeHyBug) {\n", "\n}\n");
