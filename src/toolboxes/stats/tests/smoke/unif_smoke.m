clear

import compat.*

fprintf('unifpdf(0.5)        = %.6f  (expect 1.000000)\n', unifpdf(0.5));
fprintf('unifpdf(2, 0, 5)    = %.6f  (expect 0.200000)\n', unifpdf(2, 0, 5));
fprintf('unifpdf(-1)         = %.6f  (expect 0.000000)\n', unifpdf(-1));

fprintf('unifcdf(0.3)        = %.6f  (expect 0.300000)\n', unifcdf(0.3));
fprintf('unifcdf(2, 0, 5)    = %.6f  (expect 0.400000)\n', unifcdf(2, 0, 5));
fprintf('unifcdf(10, 0, 5)   = %.6f  (expect 1.000000)\n', unifcdf(10, 0, 5));

fprintf('unifinv(0.5, -1, 1) = %.6f  (expect 0.000000)\n', unifinv(0.5, -1, 1));
fprintf('unifinv(0.95, 0, 5) = %.6f  (expect 4.750000)\n', unifinv(0.95, 0, 5));

[m, v] = unifstat(0, 5);
fprintf('unifstat(0, 5)      = [%.4f, %.4f]  (expect [2.5, 2.0833])\n', m, v);

N = 50000;
X = unifrnd(-2, 4, N, 1);
fprintf('unifrnd(-2,4) N=%d: mean=%.4f var=%.4f (expect 1.0, 3.0)\n', N, mean(X), var(X));
