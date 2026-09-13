# lang.datetime — function missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function — a fundamental MATLAB datatype)
- **Kind:** missing-fn
- **Found:** 2026-09-12 via the Play capability sweep; NOT yet listed
  in `bugs/missing.md` (inventory hole — to be added with this file).

## Symptom

`datetime` (and the arithmetic/calendar family around it) is not
registered; instants and calendar arithmetic cannot be represented.

## Repro

```matlab
clear;
d = datetime(2026, 1, 1)
d + caldays(10)
% nk: Error — VM: undefined function 'datetime' (in call to 'datetime')
% MATLAB:
%   d = datetime  01-Jan-2026
%   ans = datetime  11-Jan-2026
```

## Root cause

Not implemented.

## Suggested fix

`datetime` as a first-class datatype in the timefun layer
(`src/builtin/src/timefun/`, beside `datevec`/`dates.cpp`):
y/m/d/h/m/s storage + display formats, constructors
(`datetime(y,m,d)`, `datetime('now')`, `datetime(str, format)`),
calendar arithmetic (`caldays`/`calmonths`), comparison and `diff`.
Reuse the existing `datevec`/`datenum` machinery for conversions.
Display format default `dd-MMM-uuuu` per MATLAB.

## References

- **Guard:** deferred — missing-fn entry (feature gap, not a defect)
- `src/builtin/src/timefun/dates.cpp` (datenum/datevec machinery)
- MATLAB `doc datetime`
