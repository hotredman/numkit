#!/usr/bin/env bash
# Phase-2b compute/register splitter for toolboxes/signal single-detail-block files.
# Usage: tmp_split_sig.sh <compute.cpp relative to repo root>
# Produces <name>_reg.cpp and truncates the compute TU at its detail block.
set -euo pipefail

f="$1"
base="${f%.cpp}"
reg="${base}_reg.cpp"
# rel for the banner comment: strip "toolboxes/<lib>/src/"
rel="${f#toolboxes/*/src/}"
relbase="${rel%.cpp}"
# Auto-detect the toolbox namespace (numkit::signal / numkit::stats / …) so the
# reg wrapper + compute re-close use the file's real namespace, not a hardcode.
NS=$(grep -oE 'namespace numkit::[a-z_]+' "$f" | head -1 | sed 's/^namespace //')
NS="${NS:-numkit::signal}"

# 1:1 include swap (line count preserved → detail-block line numbers stable).
sed -i \
  -e 's#<numkit/core/engine.hpp>#<numkit/value/value.hpp>#' \
  -e 's#<numkit/core/types.hpp>#<numkit/value/error.hpp>#' \
  "$f"

Nopen=$(awk '/^namespace detail \{/{print NR; exit}' "$f")
Nclose=$(awk '/^\} \/\/ namespace detail/{print NR; exit}' "$f")
if [ -z "${Nopen:-}" ] || [ -z "${Nclose:-}" ]; then
  echo "!! $f: detail block not found (open=$Nopen close=$Nclose)" >&2
  exit 1
fi

# --- register TU ---------------------------------------------------------
{
  echo "// toolboxes/signal/src/${relbase}_reg.cpp"
  echo "//"
  echo "// CallContext register half of ${rel} (Phase 2b compute/register split)."
  echo "// Engine-coupled glue: marshals CallContext args/outs into the engine-free"
  echo "// compute API declared in the headers below. See project_layering_refactor."
  echo "#include <numkit/core/engine.hpp>"
  # all numkit/ includes from compute except core/* (signal/builtin/value/etc.)
  grep -E '^#include <numkit/' "$f" | grep -v 'numkit/core/' | sort -u
  # quoted local includes from compute (shared helpers, e.g. dist_helpers.hpp).
  # `|| true`: no quoted includes is fine (grep exit 1 would trip pipefail).
  { grep -E '^#include "' "$f" || true; } | sort -u
  echo "#include <numkit/value/error.hpp>"
  echo "#include <numkit/value/scratch.hpp>"
  echo "#include <numkit/value/span.hpp>"
  echo ""
  echo "#include <algorithm>"
  echo "#include <cctype>"
  echo "#include <cmath>"
  echo "#include <complex>"
  echo "#include <cstddef>"
  echo "#include <string>"
  echo "#include <tuple>"
  echo "#include <utility>"
  echo "#include <vector>"
  echo ""
  echo "namespace $NS {"
  echo ""
  sed -n "${Nopen},${Nclose}p" "$f"
  echo ""
  echo "} // namespace $NS"
} > "$reg"

# --- truncate compute at the detail block --------------------------------
# keep [1 .. Nopen-1], trailing-strip blank/comment lines (drops the
# "Engine adapters" banner + trailing blanks), then re-close the namespace.
awk -v n="$Nopen" '
  NR>=n{exit}
  /^[[:space:]]*$/ || /^[[:space:]]*\/\// { buf=buf $0 ORS; next }
  { printf "%s", buf; buf=""; print }
' "$f" > "$f.tmp"
printf '\n} // namespace %s\n' "$NS" >> "$f.tmp"
mv "$f.tmp" "$f"

echo "OK $f -> $reg (detail $Nopen..$Nclose)"
