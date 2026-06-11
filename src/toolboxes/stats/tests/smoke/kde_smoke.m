clear
import compat.*

fprintf('=== kde — MATLAB R2023b+ alias for ksdensity ===\n');

% Default — 100 evaluation points spread over the data range.
rng(0);
x = randn(200, 1);
[f, xi, bw] = kde(x);
fprintf('\ndefault on randn(200, 1):\n');
fprintf('   numel(f) = %d, numel(xi) = %d\n', numel(f), numel(xi));
fprintf('   bandwidth = %.4f  (Silverman rule of thumb)\n', bw);
fprintf('   peak density = %.4f  near xi = %.3f\n', max(f), xi(find(f == max(f), 1)));

% Explicit evaluation points.
fprintf('\nat fixed pts [-3, -2, ..., 3]:\n');
pts = -3:0.5:3;
fp = kde(x, pts);
disp(fp);

% Identical to ksdensity (direct alias).
fk = kde(x, pts);
fs = ksdensity(x, pts);
fprintf('\nkde vs ksdensity max diff: %g  (expect ~0)\n', max(abs(fk - fs)));

% Integral over [-10, 10] should be ~1.
fine = linspace(-10, 10, 401);
ffine = kde(x, fine);
fprintf('integral over [-10, 10] = %.4f  (expect ~1)\n', sum(ffine) * (fine(2) - fine(1)));
