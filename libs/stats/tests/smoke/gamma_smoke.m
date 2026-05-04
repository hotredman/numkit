clear

import compat.*

% Gamma distribution smoke checks (MATLAB convention: a=shape, b=scale).

% pdf(2; a=2, b=1) — Erlang-like
% f = x^(a-1) exp(-x/b) / (b^a Γ(a)) = 2 * exp(-2) / (1 * 1) = 0.270671
v1 = gampdf(2, 2, 1);
fprintf('gampdf(2, 2, 1) = %.6f  (expect 0.270671)\n', v1);

% pdf(3; a=4, b=2) = 3^3 exp(-1.5) / (16 * 6) = 27 * 0.22313 / 96 = 0.062760
v2 = gampdf(3, 4, 2);
fprintf('gampdf(3, 4, 2) = %.6f  (expect 0.062760)\n', v2);

% cdf: gamcdf(x, 1, b) = 1 - exp(-x/b) (exponential limit)
v3 = gamcdf(2, 1, 2);
fprintf('gamcdf(2, 1, 2) = %.6f  (expect %.6f)\n', v3, 1 - exp(-1));

% cdf round-trip via inv
v4 = gaminv(gamcdf(3.5, 2, 1.5), 2, 1.5);
fprintf('gaminv(gamcdf(3.5)) = %.6f  (expect 3.500000)\n', v4);

% MATLAB R2025b reference: gaminv(0.95, 2, 1) = 4.74386
v5 = gaminv(0.95, 2, 1);
fprintf('gaminv(0.95, 2, 1) = %.6f  (expect 4.743865)\n', v5);

% stat
[m, v] = gamstat(3, 2);
fprintf('gamstat(3, 2) = [%.6f, %.6f]  (expect [6.000000, 12.000000])\n', m, v);

% rnd statistics
N = 50000;
X = gamrnd(3, 2, N, 1);
fprintf('gamrnd(3,2) N=%d: mean=%.4f var=%.4f (expect 6.0, 12.0)\n', ...
    N, mean(X), var(X));
