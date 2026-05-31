clear

import compat.*

% contains/startsWith/endsWith with a cell (or string) array of patterns.
% Bug fixed 2026-05-30: these threw "Not a char array" on a cell pattern.
% MATLAB matches if ANY of the listed patterns matches. vs MATLAB R2025b.

fprintf('=== startsWith (match-any) ===\n');
fprintf('{foo,xyz} -> %s (expect true)\n',  mat2str(startsWith('foobar', {'foo','xyz'})));
fprintf('{zzz,xyz} -> %s (expect false)\n', mat2str(startsWith('foobar', {'zzz','xyz'})));

fprintf('\n=== endsWith (match-any) ===\n');
fprintf('test.m {.m,.cpp}   -> %s (expect true)\n',  mat2str(endsWith('test.m', {'.m','.cpp'})));
fprintf('test.txt {.m,.cpp} -> %s (expect false)\n', mat2str(endsWith('test.txt', {'.m','.cpp'})));

fprintf('\n=== contains (match-any) ===\n');
fprintf('{ell,xyz} -> %s (expect true)\n',  mat2str(contains('hello', {'ell','xyz'})));
fprintf('{zzz,xyz} -> %s (expect false)\n', mat2str(contains('hello', {'zzz','xyz'})));

fprintf('\n=== string-array pattern + scalar regress ===\n');
fprintf('startsWith ["foo" "xyz"] -> %s (expect true)\n', mat2str(startsWith('foobar', ["foo" "xyz"])));
fprintf('contains scalar ell      -> %s (expect true)\n', mat2str(contains('hello', 'ell')));
