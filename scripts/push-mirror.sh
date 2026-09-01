#!/usr/bin/env bash
# Push the local main to the PUBLIC GitHub mirror (hotredman/numkit).
# Per AGENTS.md push policy: the agent pushes only to origin
# (git.megahard.ru); this mirror is pushed manually by the user.
#
# Usage: scripts/push-mirror.sh [--force]

set -euo pipefail
git push github main "$@"
