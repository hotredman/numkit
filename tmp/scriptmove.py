BS = chr(92)            # backslash
DP0 = "set PROJECT_DIR=%~dp0"
DP0_PARENT = "set PROJECT_DIR=%~dp0.." + BS    # %~dp0..\

edits = {
    "scripts/build.sh":   [('cd "$(dirname "$0")"', 'cd "$(dirname "$0")/.."')],
    "scripts/test.sh":    [('cd "$(dirname "$0")"', 'cd "$(dirname "$0")/.."')],
    "scripts/dev.sh":     [('PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"',
                            'PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"')],
    "scripts/deploy.sh":  [('PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"',
                            'PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"'),
                           ('bash "${PROJECT_DIR}/build.sh"',
                            'bash "$(dirname "$0")/build.sh"')],
    "scripts/dev.bat":     [(DP0, DP0_PARENT)],
    "scripts/desktop.bat": [(DP0, DP0_PARENT)],
    "scripts/build-desktop.bat": [(DP0, DP0_PARENT),
                                  ('call "%PROJECT_DIR%build.bat"', 'call "%~dp0build.bat"')],
    "scripts/deploy.bat":  [(DP0, DP0_PARENT),
                            ('call "%PROJECT_DIR%build.bat"', 'call "%~dp0build.bat"')],
    # build.bat intentionally unchanged: no cd, PROJECT_DIR unused, relies on CWD=root.
}
for f, reps in edits.items():
    t = open(f, encoding="utf-8", newline="").read()
    for o, n in reps:
        c = t.count(o)
        assert c >= 1, f"NOT FOUND in {f}: {o!r}"
        print(f"{c}  {f}  <-  {o[:38]!r}")
        t = t.replace(o, n)
    open(f, "w", encoding="utf-8", newline="").write(t)
print("done; build.bat left unchanged by design")
