clear

import compat.*

fprintf('=== chebwin large N (BUG #35 regression guard) ===\n');
fprintf('  BUG #35: chebwin(1024, 100) used to return all-ones\n');
fprintf('  (FFT-based impl saturated). Direct cosine-IDFT must hold.\n\n');

w = chebwin(1024, 100);
fprintf('  N=1024  size=%dx%d  sum=%.4f  (MATLAB ~378)\n', ...
        size(w,1), size(w,2), sum(w));
fprintf('  peak=%.6f  (expect 1)\n', max(w));
fprintf('  min=%.6f   (expect > 0, < 0.1)\n', min(w));
fprintf('  mid=%.6f   (expect 1.0 — peak at centre)\n', w(512));
fprintf('  end=%.6f   (expect ~0 — taper)\n', w(end));
fprintf('  near-zero count (|w| < 0.1): %d / 1024  (expect ~750)\n', ...
        sum(abs(w) < 0.1));

% A "rectangular" (broken) window would have sum == 1024 and
% all-ones. Sanity-check that sum is meaningfully smaller.
if abs(sum(w) - 1024) < 1
    fprintf('  FAIL: sum ~ 1024 — window is rectangular!\n');
else
    fprintf('  PASS: window has proper main-lobe-to-sidelobe shape.\n');
end

% Symmetric? (window is even).
asym = max(abs(w - flipud(w)));
fprintf('  symmetry error: %.3g  (expect ~0)\n', asym);

% Try a few other moderately-large N to make sure even-N path works.
for N = [128, 256, 512, 2048]
    w = chebwin(N, 100);
    fprintf('  N=%4d sum=%.4f peak=%.4f min=%.6f\n', N, sum(w), max(w), min(w));
end
