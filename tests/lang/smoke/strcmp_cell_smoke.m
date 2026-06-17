clear

import compat.*

% strcmp / strcmpi / strncmp / strncmpi over CELL arrays — DEEP-PROBE
% 2026-05-31. These were char-vs-char only (threw "Not a char array" on a
% cell). MATLAB compares a cell array of strings element-wise against a
% char scalar (or another cell), returning a logical array shaped like the
% cell. Also fixed the strncmp short-string predicate (full equality when
% both strings are shorter than n). Reference: MATLAB R2025b.

fprintf('=== strcmp cell vs char ===\n');
y = strcmp({'apple','banana','apple'}, 'apple');
fprintf('strcmp({apple,banana,apple},apple) = %d %d %d  (expect 1 0 1)\n', y(1),y(2),y(3));

fprintf('\n=== strcmpi cell (case-insensitive) ===\n');
yi = strcmpi({'APPLE','x','Apple'}, 'apple');
fprintf('= %d %d %d  (expect 1 0 1)\n', yi(1),yi(2),yi(3));

fprintf('\n=== cell vs cell (element-wise) ===\n');
yy = strcmp({'a','b','c'}, {'a','x','c'});
fprintf('= %d %d %d  (expect 1 0 1)\n', yy(1),yy(2),yy(3));

fprintf('\n=== strncmp cell, n=3 ===\n');
n = strncmp({'apple','apricot','banana'}, 'app', 3);
fprintf('= %d %d %d  (expect 1 0 0)\n', n(1),n(2),n(3));

fprintf('\n=== shape follows the cell (2x1 -> 2x1) ===\n');
yc = strcmp({'x';'y'}, 'x');
fprintf('size = %dx%d  (expect 2x1)\n', size(yc,1), size(yc,2));

fprintf('\n=== short-string predicate ===\n');
fprintf('strncmp(ab,ab,5)=%d (expect 1)  strncmp(ab,abc,5)=%d (expect 0)\n', ...
        strncmp('ab','ab',5), strncmp('ab','abc',5));
