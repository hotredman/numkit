#!/usr/bin/env bash
exec python3 "$(dirname "${BASH_SOURCE[0]}")/../tools/deploy_pages.py" --type doxy "$@"
