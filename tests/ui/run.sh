#!/usr/bin/env bash
# Web UI checks: every JS module parses, the PHP bundlers lint (when php is
# present), and the core logic's unit tests pass under Node.
set -euo pipefail
cd "$(dirname "$0")/../.."
for f in html/v2/js/files/*.js tools/mock-device.js tools/screenshot.js; do
  [ -f "$f" ] && node --check "$f"
done
if command -v php >/dev/null 2>&1; then
  for f in html/v2/inc/bundle.php html/v2/css/style.php html/v2/js/javascript.php html/v1/html/*.php html/v1/js/javascript.php html/v1/css/style.php; do
    php -l "$f" >/dev/null
  done
fi
node tests/ui/test_core.js
