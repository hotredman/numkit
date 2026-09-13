# lang.table — function missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function — a fundamental MATLAB datatype)
- **Kind:** missing-fn
- **Found:** 2026-09-12 via the Play capability sweep; also blocks the
  planned reactive table editor (cell-array-backed, element-level
  dirty tracking — see the Play roadmap discussion).

## Symptom

`table` (and the whole table/timetable family) is not registered;
heterogeneous column data cannot be represented.

## Repro

```matlab
clear;
t = table([1; 2], [3; 4]);
t.Properties.VariableNames
% nk: Error — VM: undefined function 'table' (in call to 'table')
% MATLAB: ans = 1×2 cell array  {'Var1'}  {'Var2'}
```

## Root cause

Not implemented.

## Suggested fix

`table` is the first heterogeneous CONTAINER datatype: N named
columns, each its own array type (double/logical/cell/char/string),
plus `Properties` (VariableNames, RowNames, DimensionNames…). This
is the datatype the reactive table editor will back onto — design
with element-level access facts in mind (the ReactiveSession
`setParamElement` seam already exists). Start: constructor +
`Properties.VariableNames` + `()`/`.` subsetting + display;
`t(:, i)` column extraction; then the conversion family
(`table2array`/`array2table`/`cell2table`).

## References

- **Guard:** deferred — missing-fn entry (feature gap, not a defect)
- the whole family is inventoried in `bugs/missing.md` (Cell Arrays /
  Tables / Timetables sections)
- MATLAB `doc table`
