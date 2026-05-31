clear
import compat.*
% poly2rc — AR poly -> reflection coefficients + zero-lag autocorr R0.
% [k, R0] = poly2rc(a, efinal), R0 = efinal / prod(1 - k.^2).
[k, r0] = poly2rc([1 0.6 0.2 -0.1], 4);
fprintf('k  = [%.4f %.6f %.4f] (expect 0.4960 0.262626 -0.1000)\n', k(1), k(2), k(3));
fprintf('R0 = %.6f (expect 5.755727)\n', r0);

% Single-output form: just the reflection coefficients.
k2 = poly2rc([1 0.6 0.2]);
fprintf('k2 = [%.3f %.3f] (expect 0.500 0.200)\n', k2(1), k2(2));
