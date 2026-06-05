#!/usr/bin/env python3
"""
check_api.py — lint public libs/ API headers against docs/LIBRARY_API.md.

Covers the *mechanically checkable* subset of the API ruleset. Human-
judgement rules (§3 test coverage, §5 reference citations) are enforced
by the PR checklist in docs/LIBRARY_API.md, not here.

Checks, over every libs/<ns>/include/**/*.hpp header:
  §13  no `Engine` / `CallContext` by-ref/by-ptr in a public signature
  §7   `memory_resource` is passed by pointer, never by reference
  §10  no `const Value *` in a public signature

§13 exceptions (see docs/LIBRARY_API.md §13):
  * library.hpp        — the install(Engine&) registration hook.
  * libs/io/**         — file I/O needs the engine's virtual filesystem.
  * a line marked      `// lint: engine-io` — an engine text-sink / fid
                         I/O function living outside libs/io.

Run:   python tools/lint/check_api.py
Exit:  0 = clean, 1 = violations found.
"""
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ENGINE_IO_MARKER = "lint: engine-io"


def engine_bulk_exempt(rel: str) -> bool:
    """Files where an `Engine` parameter is categorically allowed."""
    return rel.endswith("library.hpp") or rel.startswith("libs/io/")


def strip_comments_and_strings(text: str) -> str:
    """Blank out // and /* */ comments and string / char literals,
    preserving newlines so reported line numbers stay accurate."""
    out = []
    i, n = 0, len(text)
    state = "code"  # code | line | block | str | chr
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == "/" and nxt == "/":
                state = "line"; out.append("  "); i += 2; continue
            if c == "/" and nxt == "*":
                state = "block"; out.append("  "); i += 2; continue
            if c == '"':
                state = "str"; out.append(" "); i += 1; continue
            if c == "'":
                state = "chr"; out.append(" "); i += 1; continue
            out.append(c); i += 1; continue
        if state == "line":
            if c == "\n":
                state = "code"; out.append("\n")
            else:
                out.append(" ")
            i += 1; continue
        if state == "block":
            if c == "*" and nxt == "/":
                state = "code"; out.append("  "); i += 2; continue
            out.append("\n" if c == "\n" else " "); i += 1; continue
        # str / chr
        if c == "\\":
            out.append("  "); i += 2; continue
        if (state == "str" and c == '"') or (state == "chr" and c == "'"):
            state = "code"
        out.append("\n" if c == "\n" else " "); i += 1
    return "".join(out)


# `Engine &` / `Engine*` etc. — a by-ref/by-ptr signature use. A bare
# `class Engine;` forward declaration has no `&` / `*` and is fine.
RX_ENGINE = re.compile(r"\b(?:Engine|CallContext)\s*[&*]")
RX_MR_REF = re.compile(r"memory_resource\s*&")
RX_VALUE_PTR = re.compile(r"const\s+Value\s*\*")


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    headers = sorted(REPO.glob("libs/*/include/**/*.hpp"))
    violations = []
    for path in headers:
        rel = path.relative_to(REPO).as_posix()
        src = path.read_text(encoding="utf-8", errors="replace")
        raw = src.splitlines()
        clean = strip_comments_and_strings(src).splitlines()
        bulk = engine_bulk_exempt(rel)
        for lineno, (raw_line, code) in enumerate(zip(raw, clean), 1):
            if RX_ENGINE.search(code):
                if not bulk and ENGINE_IO_MARKER not in raw_line:
                    violations.append((rel, lineno, "§13",
                        "Engine/CallContext in a public libs/ signature "
                        "— use FnHandle (§12), or mark `// lint: engine-io`"))
            if RX_MR_REF.search(code):
                violations.append((rel, lineno, "§7",
                    "memory_resource must be a pointer, not a reference"))
            if RX_VALUE_PTR.search(code):
                violations.append((rel, lineno, "§10",
                    "const Value* in a public signature "
                    "— use const Value& / Value::Empty"))

    for rel, lineno, sec, msg in sorted(violations):
        print(f"{rel}:{lineno}: [{sec}] {msg}")

    status = "FAIL" if violations else "OK"
    print(f"\ncheck_api: {status} — {len(headers)} public headers scanned, "
          f"{len(violations)} violation(s).")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
