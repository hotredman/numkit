clear

import compat.*

% isvarname: is the input a valid MATLAB variable name? Implemented
% 2026-05-30 (was an undefined function). vs MATLAB R2025b.

fprintf('=== valid names ===\n');
fprintf('abc -> %d (expect 1)\n', isvarname('abc'));
fprintf('a_1 -> %d (expect 1)\n', isvarname('a_1'));
fprintf('"abc" string -> %d (expect 1)\n', isvarname("abc"));

fprintf('\n=== invalid: bad characters / position ===\n');
fprintf('1abc (leading digit)     -> %d (expect 0)\n', isvarname('1abc'));
fprintf('_x   (leading underscore)-> %d (expect 0)\n', isvarname('_x'));
fprintf('a b  (space)             -> %d (expect 0)\n', isvarname('a b'));
fprintf('''''   (empty)            -> %d (expect 0)\n', isvarname(''));

fprintf('\n=== invalid: reserved keywords ===\n');
fprintf('if  -> %d (expect 0)\n', isvarname('if'));
fprintf('end -> %d (expect 0)\n', isvarname('end'));

fprintf('\n=== non-text inputs yield false (no error) ===\n');
fprintf('numeric 5  -> %d (expect 0)\n', isvarname(5));
fprintf('cell {abc} -> %d (expect 0)\n', isvarname({'abc'}));
