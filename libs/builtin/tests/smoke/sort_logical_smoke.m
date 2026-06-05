clear

import compat.*

% bugs/builtin/sort-logical.md — sort on logical input.
% MATLAB PRESERVES the logical class on the sorted VALUES; the 2nd-output
% index stays double. char is NOT coerced (sorts by code point, stays char).

[S, I] = sort(logical([0 1 0 1]));
fprintf('sort(logical([0 1 0 1])) = [%g %g %g %g]   expect [0 0 1 1], islogical=%d (expect 1)\n', ...
        S(1), S(2), S(3), S(4), islogical(S));
fprintf('  index = [%g %g %g %g]   expect [1 3 2 4], islogical(I)=%d (expect 0)\n', ...
        I(1), I(2), I(3), I(4), islogical(I));

Sd = sort(logical([1 0 1 0 0]), 'descend');
fprintf('sort(...,''descend'') = [%g %g %g %g %g]   expect [1 1 0 0 0]\n', Sd(1), Sd(2), Sd(3), Sd(4), Sd(5));

C = sort(logical([1 0; 0 1]));
fprintf('sort(logical([1 0;0 1])) col = [%g %g; %g %g]   expect [0 0; 1 1]\n', C(1,1), C(1,2), C(2,1), C(2,2));

R = sort(logical([1 0; 0 1]), 2);
fprintf('sort(...,2) row = [%g %g; %g %g]   expect [0 1; 0 1]\n', R(1,1), R(1,2), R(2,1), R(2,2));

y = sort(true);
fprintf('sort(true) = %g   expect 1, islogical=%d (expect 1)\n', y, islogical(y));

% NOTE: sort('dcba') (char) is handled separately — see sort_char_smoke.m
% (fixed 2026-06-05, bugs/builtin/sort-char.md).
