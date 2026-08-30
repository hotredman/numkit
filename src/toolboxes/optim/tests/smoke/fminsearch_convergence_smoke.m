clear

% fminsearch (Nelder-Mead) now enforces BOTH the function-value spread
% (TolFun) AND the simplex size (TolX) — so it reaches the true minimum
% (~1e-4 with the default tol), matching MATLAB R2025b. Previously only the
% f-spread was checked, so it stopped ~100x short (~1e-2).

% 2-D quadratic: min at [1, 2]
q = fminsearch(@(v) (v(1)-1)^2 + (v(2)-2)^2, [0 0]);
fprintf('quadratic  x=[%.6f %.6f]  (expect ~1, 2)\n', q(1), q(2));

% Rosenbrock from [-1.2 1]: MATLAB converges to [1.00002 1.00004]
r = fminsearch(@(v) 100*(v(2)-v(1)^2)^2 + (1-v(1))^2, [-1.2 1]);
fprintf('rosenbrock x=[%.5f %.5f]  (expect 1.00002, 1.00004 = MATLAB)\n', r(1), r(2));

% 1-D objective: min of x^2 - 2x at x = 1
o = fminsearch(@(x) x^2 - 2*x, 0);
fprintf('1-D        x=%.6f  (expect 1)\n', o);

% an explicit, tighter tol is still honoured
t = fminsearch(@(v) (v(1)-3)^2 + (v(2)+1)^2, [0 0], 1e-8);
fprintf('tight tol  x=[%.6f %.6f]  (expect 3, -1)\n', t(1), t(2));
