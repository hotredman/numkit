import compat.*

% --- mscohere(x, x) = 1 everywhere (auto-coherence) ---
rng(42);
x = randn(1024, 1);
[Cxx, F] = mscohere(x, x);
fprintf('--- mscohere(x, x) ---\n');
fprintf('  size(Cxx) = %d (expect 129 = nfft/2 + 1 with default)\n', numel(Cxx));
fprintf('  min(Cxx) = %.6f, max(Cxx) = %.6f (expect both ≈ 1.0)\n\n', ...
    min(Cxx), max(Cxx));

% --- mscohere(x, indep_y) ≈ 1/nSegments at each freq when independent ---
y = randn(1024, 1);
[Cxy, F] = mscohere(x, y);
fprintf('--- mscohere(x, randn) — independent ---\n');
fprintf('  mean(Cxy) = %.4f (expect ≈ small / per-segment count)\n', mean(Cxy));
fprintf('  max(Cxy)  = %.4f\n\n', max(Cxy));

% --- y = filter response of x: coherence should be ≈ 1 across band ---
% LTI: y = filter(h, 1, x) for a simple FIR.
h = ones(1, 5) / 5;
y_lti = filter(h, 1, x);
[Clti, F] = mscohere(x, y_lti);
fprintf('--- mscohere(x, filter(h, 1, x)) — LTI relation ---\n');
fprintf('  mean(Clti) = %.4f (expect ≈ 1.0 across band)\n', mean(Clti));

% --- cpsd(x, x) magnitude == pwelch(x) ---
[Pxy, F] = cpsd(x, x);
[Pxx, F2] = pwelch(x);
fprintf('\n--- cpsd(x, x) magnitude ≈ pwelch(x) ---\n');
fprintf('  size match : %d == %d\n', numel(Pxy), numel(Pxx));
fprintf('  max|abs(Pxy) - Pxx| = %.6e (expect ≈ 0 — auto-PSD identity)\n', ...
    max(abs(abs(Pxy) - Pxx)));

% --- cpsd has same length as F vector ---
fprintf('\n  F(1) = %.4f (expect 0)\n', F(1));
fprintf('  F(end) = %.4f (expect pi ≈ 3.1416)\n', F(end));
