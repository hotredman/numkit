clear

import compat.*

% Math primitives + reductions batch — audit ТЗ closure 2026-05-09.
% cospi/sinpi + deg2rad/rad2deg + eps + cumsum/cumprod/diff +
% diag + prod/sum.

fprintf('cospi(0.5)         = %g  (expect 0)\n',     cospi(0.5));
fprintf('sinpi(1)           = %g  (expect 0)\n',     sinpi(1));
fprintf('deg2rad(180)       = %.15f  (expect pi)\n', deg2rad(180));
fprintf('rad2deg(pi)        = %.15f  (expect 180)\n', rad2deg(pi));
fprintf('eps(1)             = %g\n',                  eps(1));
disp('cumsum([1..5]):'); disp(cumsum([1 2 3 4 5]));
disp('cumprod([1..5]):'); disp(cumprod([1 2 3 4 5]));
disp('diff([1 4 9 16 25]):'); disp(diff([1 4 9 16 25]));
disp('diag([1 2 3]):'); disp(diag([1 2 3]));
fprintf('sum([1..5]) = %g, prod([1..5]) = %g\n', sum(1:5), prod(1:5));
