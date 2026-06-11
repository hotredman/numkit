clear

import compat.*

% reverse / erase / count on a CELL array — DEEP-PROBE 2026-05-31. The first
% (string) argument was rejected as "Not a char array". MATLAB processes a
% cell str element-wise: reverse/erase return a cell of char vectors (same
% shape); count returns a DOUBLE array (same shape). Pattern args may also be
% cells (erase removes every listed substring; count sums across patterns).
% Reference: MATLAB R2025b.

fprintf('=== reverse over a cell ===\n');
r = reverse({'abc','de'});
fprintf('class=%s -> {%s, %s}  (expect cba, ed)\n', class(r), r{1}, r{2});

fprintf('\n=== erase over a cell (+ cell match) ===\n');
e = erase({'a1b','c2'}, '1');
fprintf('erase 1 -> {%s, %s}  (expect ab, c2)\n', e{1}, e{2});
ec = erase({'a1b2','c2'}, {'1','2'});
fprintf('erase {1,2} -> {%s, %s}  (expect ab, c)\n', ec{1}, ec{2});

fprintf('\n=== count over a cell -> double array ===\n');
c = count({'aXbX','XcX'}, 'X');
fprintf('class=%s -> %s  (expect double [2 2])\n', class(c), mat2str(c));
fprintf('count cell-pattern abcabc {a,bc} = %g  (expect 4)\n', count('abcabc', {'a','bc'}));

fprintf('\n=== shape preserved + scalar paths unchanged ===\n');
rc = reverse({'ab';'cd'});
fprintf('reverse column -> size %dx%d  (expect 2x1)\n', size(rc,1), size(rc,2));
fprintf('scalar reverse(hello) = %s ; count(aXbX,X) = %g\n', reverse('hello'), count('aXbX','X'));
