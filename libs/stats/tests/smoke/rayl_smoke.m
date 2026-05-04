clear

import compat.*

% raylpdf(2, 1) = 2 * exp(-2) = 0.2707
fprintf('raylpdf(2, 1)       = %.6f  (expect 0.270671)\n', raylpdf(2, 1));

% raylcdf(1, 1) = 1 - exp(-0.5) = 0.3935
fprintf('raylcdf(1, 1)       = %.6f  (expect 0.393469)\n', raylcdf(1, 1));

% raylinv: round-trip
fprintf('raylinv(raylcdf(2.5, 3), 3) = %.6f  (expect 2.500000)\n', raylinv(raylcdf(2.5, 3), 3));

% raylstat(2): mean = 2*sqrt(π/2) = 2.5066, var = (4-π)/2 * 4 = 1.7168
[m, v] = raylstat(2);
fprintf('raylstat(2)         = [%.4f, %.4f]  (expect [2.5066, 1.7168])\n', m, v);

% rnd
N = 50000;
X = raylrnd(2, N, 1);
fprintf('raylrnd(2) N=%d: mean=%.4f var=%.4f (expect 2.5066, 1.7168)\n', N, mean(X), var(X));
