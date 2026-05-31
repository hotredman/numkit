clear

import compat.*

% count/erase with a cell (or string) array of patterns. Bug fixed
% 2026-05-30: both threw "Not a char array" on a cell pattern argument.
% vs MATLAB R2025b.

fprintf('=== count: sum per-pattern non-overlapping occurrences ===\n');
fprintf('count(abcabc,{a,c}) = %d (expect 4)\n', count('abcabc', {'a','c'}));
fprintf('count(abcABC,{a,b,c}) = %d (expect 3, case-sensitive)\n', count('abcABC', {'a','b','c'}));

fprintf('\n=== erase: remove every occurrence of each pattern, in order ===\n');
fprintf('erase(a-b_c,{-,_}) = [%s] (expect abc)\n', erase('a-b_c', {'-','_'}));
fprintf('erase(hello world,{ll,rl}) = [%s] (expect heo wod)\n', erase('hello world', {'ll','rl'}));

fprintf('\n=== string-array pattern list ===\n');
fprintf('erase(abcd,["b" "c"]) = [%s] (expect ad)\n', erase('abcd', ["b" "c"]));

fprintf('\n=== scalar pattern unchanged ===\n');
fprintf('count(aaaa,aa) = %d (expect 2)\n', count('aaaa', 'aa'));
fprintf('erase(hello,l) = [%s] (expect heo)\n', erase('hello', 'l'));
