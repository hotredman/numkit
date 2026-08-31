# graphics.legend — cell-array labels and 'Location' name-value rejected ("Not a char array")

- **Status:** 🔴 OPEN
- **Severity:** P2 (works in MATLAB, refused in numkit; the canonical legend form)
- **Kind:** stub
- **Found:** 2026-08-31 via fieldtest portion 1 (mdadams book, example_9.m)

## Symptom

`legend` accepts only char-vector labels. Passing labels as a CELL array
(the idiomatic multi-line label form) or the `'Location'` name-value pair
throws.

## Repro (self-contained)

```matlab
clear;
x = 0:pi/10:2*pi; plot(x, sin(x)); hold on; plot(x, cos(x));
names = {'sine', 'cosine'};
legend(names, 'Location', 'northeast');
disp('ok')
% numkit:  Error: Not a char array (in call to 'legend')
% MATLAB R2025b: ok  (legend attached, north-east corner)
```

## Root cause

`legend` argument handling expects char arrays only: no cell-of-char
decomposition, no name-value pair parsing.

## Suggested fix

Accept `legend(cellOfChar [, name, value, ...])` (and comma-separated
chars as today). Minimum honourable subset: cell labels + 'Location'
(the values: N/S/E/NE/... /best/bestoutside — same set subplot/title
config already uses elsewhere in the graphics config model).

## References

- **Guard:** `DISABLED_LegendCellAndNameValue` in
  `src/graphics/tests/figure_test.cpp`.
