clear
import compat.*

% resample(x, p, q) — rational-factor (multirate) resampling. Designs a
% Kaiser-windowed least-squares anti-alias FIR (firls + kaiser), applies it
% via the polyphase upfirdn, compensates the group delay and trims to
% ceil(Lx*p/q) samples. bugs/signal/resample-values.

% 3/2 ramp (the bug repro): now tracks the 1..6 ramp at the new rate.
y = resample([1 2 3 4 5 6], 3, 2);
fprintf('resample([1..6], 3, 2) (len %d, sum %.4f):\n', numel(y), sum(y));
fprintf('  %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f\n', y);
fprintf('  (MATLAB: 1.00061 1.80791 2.16807 3.00182 3.94099 3.96567 5.00303 6.56811 4.24029)\n');

% Upsample 2/1 and downsample 1/2.
u = resample([1 2 3 4 5 6 7 8], 2, 1);
fprintf('resample 2/1: len %d (expect 16), sum %.5f\n', numel(u), sum(u));
d = resample([1 2 3 4 5 6 7 8], 1, 2);
fprintf('resample 1/2: len %d (expect 4),  sum %.5f\n', numel(d), sum(d));

% DC level survives (settled interior ~ input level).
yd = resample(5 * ones(1, 100), 3, 2);
fprintf('DC interior resample(5,3,2): y(75) = %.6f  (expect ~5)\n', yd(75));

% GCD reduction: 4/2 == 2/1.
a = resample([1 2 3 4 5 6], 4, 2);
b = resample([1 2 3 4 5 6], 2, 1);
fprintf('GCD reduce 4/2 == 2/1: max diff = %.2e  (expect 0)\n', max(abs(a - b)));
