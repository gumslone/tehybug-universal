#!/usr/bin/env bash
# Run clang-tidy over the host-compilable firmware logic. Uses the root
# .clang-tidy for the check set and header filter; any remaining warning fails.
set -euo pipefail

cd "$(dirname "$0")/.."

TIDY="${CLANG_TIDY:-clang-tidy}"
INC="-Itests/shims -Itests -Ilibraries/EepromFS-main"
EXTRA=""
# macOS: Homebrew clang-tidy needs the SDK so libc++ headers resolve
if [ "$(uname)" = "Darwin" ]; then
  EXTRA="-isysroot $(xcrun --show-sdk-path)"
fi

rc=0
for t in tests/test_*.cpp; do
  echo "==> clang-tidy $t"
  "$TIDY" "$t" --quiet --warnings-as-errors='*' -- \
    -std=c++17 $EXTRA $INC || rc=1
done
[ $rc -eq 0 ] && echo "clang-tidy: clean"
exit $rc
