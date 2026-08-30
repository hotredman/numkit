#!/usr/bin/env python3
"""bugs_audit.py — mechanical enforcement of the bug-catalog <-> guard-test protocol.

Companion to bugs_tally.py (which shows state; this one checks invariants).

Rules (static):
  R1 every bugs/opened/**/*.md references at least one existing DISABLED_
     guard (a `DISABLED_<Name>` token matching a real test) OR carries an
     explicit '**Guard:** deferred' line with a reason;
  R2 no bugs/closed/**/*.md references a guard that is still DISABLED
     (the chol/unique regression class: fixed bug + never-enabled guard);
  R3 every DISABLED_ guard is referenced by an open bug OR marked BY-DESIGN
     in a comment (a documented non-bug limitation) — no orphans;
  R4 an md may only reference guards that exist (catches renamed tests).

Guard-run mode (add --guards <numkit_gtest.exe>):
  R5 every non-BY-DESIGN guard must FAIL under --gtest_also_run_disabled_tests
     (it reproduces its open bug). A passing guard means the bug was silently
     fixed (enable the guard, close the bug) or the guard is stale.

Exit code 1 on any violation. Run before closing a session; hook into CI
when it exists.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEST_RE = re.compile(r'TEST_[FP]\(\s*(\w+)\s*,\s*(DISABLED_\w+)\s*\)')
GUARD_TOKEN = re.compile(r'DISABLED_[A-Za-z0-9_]+')


def inventory_guards():
    """{test_name: (file, suite, by_design)}"""
    guards = {}
    for f in list(ROOT.glob('src/**/*_test.cpp')) + list(ROOT.glob('tests/**/*_test.cpp')):
        lines = f.read_text(encoding='utf-8', errors='replace').splitlines()
        for i, line in enumerate(lines):
            m = TEST_RE.search(line)
            if not m:
                continue
            suite, name = m.group(1), m.group(2)
            context = ' '.join(lines[max(0, i - 6):i + 1])
            by_design = 'BY-DESIGN' in context
            guards.setdefault(name, (str(f.relative_to(ROOT)), suite, by_design))
    return guards


def bug_files(tree):
    return sorted((ROOT / 'bugs' / tree).glob('*/*.md'))


def audit(guards):
    v = []
    open_refs = {}  # guard name -> [md, ...]
    deferred = []

    for md in bug_files('opened'):
        rel = md.relative_to(ROOT / 'bugs')
        txt = md.read_text(encoding='utf-8', errors='replace')
        tokens = set(GUARD_TOKEN.findall(txt))
        for t in tokens:
            if t not in guards:
                v.append(f"R4 {rel}: references unknown guard '{t}' (renamed test?)")
            else:
                open_refs.setdefault(t, []).append(str(rel))
        if not tokens:
            if re.search(r'\*\*Guard:\*\* *deferred', txt, re.I):
                deferred.append(str(rel))
            else:
                v.append(f"R1 {rel}: no DISABLED_ guard reference and no '**Guard:** deferred' line")

    for md in bug_files('closed'):
        rel = md.relative_to(ROOT / 'bugs')
        txt = md.read_text(encoding='utf-8', errors='replace')
        for t in set(GUARD_TOKEN.findall(txt)):
            if t in guards and t not in open_refs:
                v.append(f"R2 {rel}: guard '{t}' is STILL DISABLED though the bug is closed "
                         f"(enable it — or the fix regressed; if the fix regressed, file "
                         f"the reopen bug and let IT reference the guard)")

    for name, (file, suite, by_design) in sorted(guards.items()):
        if by_design:
            continue
        if name not in open_refs:
            v.append(f"R3 {file}: guard '{name}' belongs to no open bug (file one, "
                     f"or enable the guard if its bug was fixed)")

    return v, open_refs, deferred


def run_guards(guards, exe):
    v = []
    live = {n: (f, s) for n, (f, s, bd) in guards.items() if not bd}
    if not live:
        return v
    filt = ':'.join(f'*{s}.{n}*' for n, (f, s) in live.items())
    p = subprocess.run([str(exe), '--gtest_also_run_disabled_tests',
                        f'--gtest_filter={filt}'],
                       capture_output=True, text=True, timeout=1800)
    failed, ok = set(), set()
    for line in (p.stdout + p.stderr).splitlines():
        m = re.search(r'\[\s+(FAILED|OK)\s+\] \S+\.(DISABLED_\w+)', line)
        if m:
            (failed if m.group(1) == 'FAILED' else ok).add(m.group(2))
    for name in live:
        if name in ok:
            v.append(f"R5 guard '{name}' PASSES while its bug is open — "
                     f"silently fixed (enable + close) or stale guard")
        elif name not in failed:
            v.append(f"R5 guard '{name}' did not run (filter/output mismatch)")
    return v


def main():
    args = sys.argv[1:]
    guards = inventory_guards()
    violations, open_refs, deferred = audit(guards)
    print(f"guards inventoried: {len(guards)} "
          f"({sum(1 for _, _, b in guards.values() if b)} BY-DESIGN)")
    print(f"open bugs referencing guards: {len(open_refs)}, deferred: {len(deferred)}")

    if '--guards' in args:
        i = args.index('--guards')
        exe = Path(args[i + 1]) if len(args) > i + 1 else None
        if not exe or not exe.exists():
            sys.exit('--guards needs a path to numkit_gtest(.exe)')
        violations += run_guards(guards, exe)

    if violations:
        print(f"\n{len(violations)} VIOLATION(S):")
        for x in violations:
            print('  -', x)
        sys.exit(1)
    print('\nprotocol clean: every bug guarded or deferred, every guard owned, '
          'no closed bug with a disabled guard')


if __name__ == '__main__':
    main()
