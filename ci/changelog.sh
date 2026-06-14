#!/usr/bin/env bash
# Prepend a release section to CHANGELOG.md from the commits since the previous
# release tag, and write the same notes to release-notes.md for the GitHub
# release body. Run by the build workflow before it commits/releases.
#
# Usage: ci/changelog.sh <new-version-tag>
set -euo pipefail

cd "$(dirname "$0")/.."

TAG="${1:?usage: ci/changelog.sh <tag>}"
DATE="$(date -u +%Y-%m-%d)"

# The new tag is created after this runs, so the latest existing v* tag is the
# previous release. Empty on the very first release -> use the whole history.
PREV="$(git tag -l 'v*' --sort=-creatordate | head -n1 || true)"
RANGE="HEAD"
[ -n "$PREV" ] && RANGE="$PREV..HEAD"

# User-facing commit subjects only: drop merges, the CI's own binary/changelog
# commits, and anything marked [skip ci].
COMMITS="$(git log "$RANGE" --no-merges --format='- %s' \
  | grep -viE '\[skip ci\]|^- (Merge |Update changelog|Build firmware binaries)' \
  || true)"
[ -z "$COMMITS" ] && COMMITS="- Maintenance and build updates"

# Notes for the GitHub release body.
printf '%s\n' "$COMMITS" > release-notes.md

# Prepend "## <tag> (<date>)" above the first existing "## " heading, keeping
# the title + intro at the top.
CL=CHANGELOG.md
HEADER="$(awk '/^## /{exit} {print}' "$CL")"
REST="$(awk '/^## /{f=1} f{print}' "$CL")"
{
  printf '%s\n\n' "$HEADER"
  printf '## %s (%s)\n\n%s\n\n' "$TAG" "$DATE" "$COMMITS"
  printf '%s\n' "$REST"
} > "$CL"

echo "==> CHANGELOG.md updated for $TAG ($([ -n "$PREV" ] && echo "since $PREV" || echo "initial release"))"
