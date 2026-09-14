#!/usr/bin/env bash
# Push the local main branch to the public GitHub mirror (hotredman/numkit).
# Usage: ./scripts/linux/code-publish.sh [--force]
set -euo pipefail
cd "$(dirname "$0")/../.."
git push github main "$@"
