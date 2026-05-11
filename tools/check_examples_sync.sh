#!/usr/bin/env bash
# check_examples_sync.sh
#
# Enforces that docs/examples/ and ide/public/examples/ stay in sync.
# Both trees ship the same .m demo files + manifest.json — the first
# is browsable on GitHub for documentation, the second is what the
# Vite build bundles into the IDE. Drift between them silently hides
# new demos from one of the two surfaces.
#
# This was hit on 2026-05-11: commit b37abbc9 added 9 new demos +
# 3 new folders only to ide/public/examples/, and a user noticed the
# Benchmark / 3D_Volume / Computational_Geometry folders looked
# different between the GitHub README links and the IDE Examples
# panel. Synced manually + this guard added.
#
# Usage: tools/check_examples_sync.sh
# Exit 0 when in sync, 1 otherwise (with a list of differences).

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
A="$ROOT/docs/examples"
B="$ROOT/ide/public/examples"

if [[ ! -d "$A" || ! -d "$B" ]]; then
    echo "FAIL: one of the example trees is missing." >&2
    [[ ! -d "$A" ]] && echo "  missing: $A" >&2
    [[ ! -d "$B" ]] && echo "  missing: $B" >&2
    exit 1
fi

# Compare manifest files first (fast check, common drift point).
if ! diff -q "$A/manifest.json" "$B/manifest.json" >/dev/null 2>&1; then
    echo "FAIL: docs/examples/manifest.json and ide/public/examples/manifest.json differ."
    diff "$A/manifest.json" "$B/manifest.json" | head -30
    echo "..."
    echo "Fix: copy the canonical manifest (usually ide/public/examples/) over the other."
    exit 1
fi

# Compare folder structure.
folders_a="$(cd "$A" && find . -maxdepth 1 -mindepth 1 -type d | sort)"
folders_b="$(cd "$B" && find . -maxdepth 1 -mindepth 1 -type d | sort)"
if [[ "$folders_a" != "$folders_b" ]]; then
    echo "FAIL: example folder sets differ."
    diff <(echo "$folders_a") <(echo "$folders_b")
    exit 1
fi

# Compare files inside each folder.
mismatch=0
while IFS= read -r dir; do
    name="$(basename "$dir")"
    files_a="$(cd "$A/$name" && find . -maxdepth 1 -mindepth 1 -type f | sort)"
    files_b="$(cd "$B/$name" && find . -maxdepth 1 -mindepth 1 -type f | sort)"
    if [[ "$files_a" != "$files_b" ]]; then
        echo "FAIL: $name file set differs."
        diff <(echo "$files_a") <(echo "$files_b")
        mismatch=1
    fi
done < <(cd "$A" && find . -maxdepth 1 -mindepth 1 -type d)

if (( mismatch )); then
    echo
    echo "Fix: rsync ide/public/examples/ ↔ docs/examples/ to make them match."
    exit 1
fi

echo "OK: docs/examples and ide/public/examples are in sync."
