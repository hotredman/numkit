#!/usr/bin/env bash
# check_vfs_invariant.sh
#
# Enforces the VFS invariant: any filesystem access from core/ or libs/
# must go through numkit::VirtualFS, never directly via std::filesystem,
# stdio FILE*, fstream, or OS-native APIs.
#
# The only exception is the VFS implementation itself (FilesystemFS,
# CallbackFS, etc.), explicitly whitelisted below.
#
# Tests (*/tests/*) and bench harnesses (bench*/) are out of scope —
# they aren't compiled into the WASM/native runtime, so direct FS use
# in test setup is allowed.
#
# Usage: tools/check_vfs_invariant.sh
# Exit:  0 on clean, 1 on violation (lists offending lines).

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Files explicitly allowed to use direct FS APIs.
# These are VFS backend implementations + utility headers that the
# VFS implementation depends on.
ALLOW_LIST=(
    "core/src/vfs.cpp"
    "core/include/numkit/core/vfs.hpp"
)

# Forbidden include patterns (FS-related headers).
FORBIDDEN_INCLUDES='<filesystem>|<fstream>|<sys/stat\.h>|<dirent\.h>|<unistd\.h>'

# Forbidden symbols (FS-related stdlib + OS APIs).
# Note: <cstdio> is allowed because std::snprintf is used widely
# for string formatting; only stdio FS-functions are forbidden below.
FORBIDDEN_SYMBOLS='\bstd::filesystem\b|\bstd::ifstream\b|\bstd::ofstream\b|\bstd::fstream\b|\bstd::fopen\b|\bstd::fclose\b|\bstd::fread\b|\bstd::fwrite\b|\bstd::fprintf\b|\bstd::fscanf\b|\bstd::fgets\b|\bstd::fputs\b|\bstd::tmpfile\b|\bstd::tmpnam\b|\bstd::remove\b|\bstd::rename\b|::fopen\s*\(|::fclose\s*\(|::fread\s*\(|::fwrite\s*\(|\bCreateFileA\b|\bCreateFileW\b|\bMapViewOfFile\b|\b_open\b|\b_wopen\b|\b_wfopen\b|\bopendir\b|\breaddir\b|\bgetcwd\b|\bchdir\b|\bDeleteFileA\b|\bDeleteFileW\b|\bMoveFileA\b|\bMoveFileW\b|\bCreateDirectoryA\b|\bCreateDirectoryW\b'

# Build whitelist regex for allowed files.
WHITELIST_REGEX=""
for f in "${ALLOW_LIST[@]}"; do
    if [ -z "$WHITELIST_REGEX" ]; then
        WHITELIST_REGEX="$f"
    else
        WHITELIST_REGEX="$WHITELIST_REGEX|$f"
    fi
done

violations=0

scan_dir() {
    local dir="$1"
    local pattern="$2"
    local label="$3"

    # Use grep -rn with extended regex; exclude tests/ and bench/.
    # rg would be cleaner but bash-only for portability.
    if [ ! -d "$ROOT/$dir" ]; then
        return
    fi

    while IFS= read -r line; do
        # line format: file:lineno:content
        local file="${line%%:*}"
        # strip ROOT prefix
        local rel="${file#$ROOT/}"
        # normalize separators (Windows)
        rel="${rel//\\//}"

        # Skip tests and bench
        case "$rel" in
            */tests/*) continue ;;
            */test/*) continue ;;
            */bench/*) continue ;;
            */benchmarks/*) continue ;;
        esac

        # Skip whitelisted files
        if echo "$rel" | grep -qE "^($WHITELIST_REGEX)$"; then
            continue
        fi

        echo "VIOLATION ($label): $line"
        violations=$((violations + 1))
    done < <(grep -rEn "$pattern" "$ROOT/$dir" 2>/dev/null \
             --include='*.cpp' --include='*.hpp' --include='*.cc' --include='*.h' \
             --include='*.cxx' --include='*.hxx' || true)
}

echo "Scanning core/ and libs/ for VFS invariant violations..."

scan_dir "core" "$FORBIDDEN_INCLUDES" "include"
scan_dir "core" "$FORBIDDEN_SYMBOLS"  "symbol"
scan_dir "libs" "$FORBIDDEN_INCLUDES" "include"
scan_dir "libs" "$FORBIDDEN_SYMBOLS"  "symbol"

if [ "$violations" -eq 0 ]; then
    echo "OK: no VFS invariant violations."
    exit 0
fi

echo ""
echo "FAIL: $violations violation(s) found."
echo "All filesystem access from core/ and libs/ must go through numkit::VirtualFS."
echo "If you have a legitimate reason (e.g., a new VFS backend), add the file"
echo "to ALLOW_LIST in tools/check_vfs_invariant.sh."
exit 1
