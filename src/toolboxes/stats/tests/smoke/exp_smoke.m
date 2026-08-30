clear

% exppdf(x, mu) = exp(-x/mu) / mu
% pdf(0, 2) = 0.5
fprintf('exppdf(0, 2) = %.6f  (expect 0.500000)\n', exppdf(0, 2));
fprintf('exppdf(2, 2) = %.6f  (expect 0.183940)\n', exppdf(2, 2));

% expcdf(x, mu) = 1 - exp(-x/mu)
fprintf('expcdf(2, 2) = %.6f  (expect 0.632121)\n', expcdf(2, 2));
fprintf('expcdf(Inf, 1) = %.6f  (expect 1.000000)\n', expcdf(1e10, 1));

% expinv: round-trip
fprintf('expinv(expcdf(3, 2), 2) = %.6f  (expect 3.000000)\n', expinv(expcdf(3, 2), 2));

% expstat
[m, v] = expstat(5);
fprintf('expstat(5) = [%.6f, %.6f]  (expect [5, 25])\n', m, v);

% rnd
N = 50000;
X = exprnd(3, N, 1);
fprintf('exprnd(3) N=%d: mean=%.4f var=%.4f (expect 3.0, 9.0)\n', N, mean(X), var(X));
