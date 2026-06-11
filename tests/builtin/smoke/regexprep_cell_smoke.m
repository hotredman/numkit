clear

import compat.*

% regexprep with CELL-ARRAY arguments — DEEP-PROBE 2026-05-31. regexprep
% threw "s, pat, rep must be strings" on any cell input. MATLAB semantics:
%  - a cell STR processes element-wise -> a cell of char vectors (same shape)
%  - a cell PATTERN is a LIST applied SEQUENTIALLY to each string (result of
%    one pattern feeds the next) -- this CHAINS, unlike strrep's broadcast
%  - a single replacement is recycled across all patterns; otherwise the
%    replacement count must equal the pattern count
% Reference: MATLAB R2025b.

fprintf('=== cell str, scalar pattern ===\n');
r = regexprep({'foo123','bar45'}, '\d+', '#');
fprintf('class=%s n=%d -> {%s, %s}  (expect foo#, bar#)\n', class(r), numel(r), r{1}, r{2});

fprintf('\n=== scalar str + cell pattern (sequential chaining) ===\n');
q = regexprep('a1b2', {'\d','[ab]'}, {'#','@'});
fprintf('class=%s val=%s  (expect char @#@#)\n', class(q), q);

fprintf('\n=== single replacement recycled across patterns ===\n');
fprintf('%s  (expect ZZZZ)\n', regexprep('a1b2', {'\d','[ab]'}, 'Z'));

fprintf('\n=== cell str + cell pattern: list applied to EACH ===\n');
c = regexprep({'a1','b2'}, {'\d','[ab]'}, {'#','@'});
fprintf('{%s, %s}  (expect @#, @#)\n', c{1}, c{2});

fprintf('\n=== ignorecase option across a cell ===\n');
ic = regexprep({'AbC','xyZ'}, '[a-z]', '_', 'ignorecase');
fprintf('{%s, %s}  (expect ___, ___)\n', ic{1}, ic{2});

fprintf('\n=== column-cell shape preserved ===\n');
cc = regexprep({'a1';'b2'}, '\d', '#');
fprintf('size %dx%d, cc{2}=%s  (expect 2x1, b#)\n', size(cc,1), size(cc,2), cc{2});

fprintf('\n=== scalar (no-cell) path unchanged ===\n');
fprintf('%s  (expect helloXworld)\n', regexprep('hello42world', '\d+', 'X'));
