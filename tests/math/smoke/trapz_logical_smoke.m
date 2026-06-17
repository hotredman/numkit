clear

import compat.*

% bugs/builtin/trapz-logical.md — trapz on logical input.
% MATLAB promotes a logical X and/or Y to double (class NOT preserved).

u = trapz(logical([1 0 1 1]));
fprintf('trapz(logical([1 0 1 1])) = %g   expect 2, islogical=%d (expect 0)\n', u, islogical(u));

nx = trapz([1 3 4 7], logical([1 0 1 1]));
fprintf('trapz([1 3 4 7], logical([1 0 1 1])) = %g   expect 4.5\n', nx);

lx = trapz(logical([0 1 1 1]), [1 2 3 4]);
fprintf('trapz(logical([0 1 1 1]), [1 2 3 4]) = %g   expect 1.5  (logical X promoted)\n', lx);

c = trapz(logical([1 0; 1 1]));
fprintf('trapz(logical([1 0;1 1])) col = [%g %g]   expect [1 0.5]\n', c(1), c(2));

r = trapz(logical([1 0; 1 1]), 2);
fprintf('trapz(...,2) row = [%g %g]   expect [0.5 1]\n', r(1), r(2));

fprintf('trapz(true) = %g   expect 0\n', trapz(true));
fprintf('trapz(logical([])) = %g   expect 0\n', trapz(logical([])));
