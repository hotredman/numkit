clear

import compat.*

% replace with a cell (or string) array of OLD patterns. Bug fixed
% 2026-05-30: replace routed through strrep and threw "Not a char array"
% on a cell. vs MATLAB R2025b.

fprintf('=== single NEW applies to every OLD ===\n');
fprintf('replace(a-b_c,{-,_},X) = [%s] (expect aXbXc)\n', replace('a-b_c', {'-','_'}, 'X'));

fprintf('\n=== paired OLD/NEW ===\n');
fprintf('replace(a-b_c,{-,_},{P,Q}) = [%s] (expect aPbQc)\n', replace('a-b_c', {'-','_'}, {'P','Q'}));

fprintf('\n=== single left-to-right pass (no chain-replacement) ===\n');
fprintf('replace(ab,{a,b},{b,c}) = [%s] (expect bc, NOT cc)\n', replace('ab', {'a','b'}, {'b','c'}));

fprintf('\n=== first-in-list match wins ===\n');
fprintf('replace(abc,{a,ab},{X,Y}) = [%s] (expect Xbc)\n', replace('abc', {'a','ab'}, {'X','Y'}));
fprintf('replace(abc,{ab,a},{Y,X}) = [%s] (expect Yc)\n', replace('abc', {'ab','a'}, {'Y','X'}));

fprintf('\n=== string-array list + scalar regress ===\n');
fprintf('replace(a-b_c,["-" "_"],"X") = [%s] (expect aXbXc)\n', replace('a-b_c', ["-" "_"], "X"));
fprintf('replace(abc,b,X) = [%s] (expect aXc)\n', replace('abc', 'b', 'X'));
