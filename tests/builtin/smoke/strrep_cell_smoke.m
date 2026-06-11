clear

import compat.*

% strrep with CELL-ARRAY arguments — DEEP-PROBE 2026-05-31. strrep threw
% "Not a char array" on any cell input. MATLAB: any cell argument => a cell
% of char vectors; non-cell char/string scalars broadcast to every element.
% NOTE this differs from `replace` (which chains multiple patterns into ONE
% string). strrep broadcasts each pattern into a SEPARATE result element.
% Reference: MATLAB R2025b.

fprintf('=== cell str, scalar old/new ===\n');
c = strrep({'hello','world','book'}, 'o', 'O');
fprintf('class=%s n=%d -> {%s, %s, %s}  (expect hellO, wOrld, bOOk)\n', ...
        class(c), numel(c), c{1}, c{2}, c{3});

fprintf('\n=== scalar str + cell pattern (broadcast, not chained) ===\n');
c2 = strrep('aXbYc', {'X','Y'}, {'-','='});
fprintf('n=%d -> {%s, %s}  (expect a-bYc, aXb=c)\n', numel(c2), c2{1}, c2{2});

fprintf('\n=== all-cell element-wise ===\n');
c3 = strrep({'aa','bb'}, {'a','b'}, {'X','Y'});
fprintf('{%s, %s}  (expect XX, YY)\n', c3{1}, c3{2});

fprintf('\n=== shape preserved (column cell) ===\n');
cc = strrep({'ax';'bx'}, 'x', 'Z');
fprintf('size %dx%d, cc{2}=%s  (expect 2x1, bZ)\n', size(cc,1), size(cc,2), cc{2});

fprintf('\n=== scalar (no-cell) path unchanged ===\n');
fprintf('%s  (expect mISSISSippi)\n', strrep('mississippi', 'iss', 'ISS'));
