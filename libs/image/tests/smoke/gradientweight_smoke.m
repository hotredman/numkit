clear
import compat.*

% gradientweight — DoG-gradient pixel weights for FMM segmentation.
% Reference values from MATLAB R2025b on magic(8)/100.

I = double(magic(8)) / 100;

fprintf('=== default (sigma=1.5, P=3, K=0.25) ===\n');
W1 = gradientweight(I);
fprintf('W1(4,4) = %.6f (expect 0.326575)\n', W1(4,4));
fprintf('W1(1,1) = %.4g  (expect 0.001 — floored below cutoff)\n', W1(1,1));
fprintf('W1(8,8) = %.4g  (expect 0.001)\n', W1(8,8));

fprintf('\n=== sigma override (2.0 scalar) ===\n');
W2 = gradientweight(I, 2.0);
fprintf('W2(4,4) = %.8f (expect 0.99999460)\n', W2(4,4));

fprintf('\n=== anisotropic sigma [1.5 2.5] (MATLAB-bug-compatible) ===\n');
W5 = gradientweight(I, [1.5 2.5]);
fprintf('W5(4,4) = %.6f (expect 0.548647)\n', W5(4,4));

fprintf('\n=== RolloffFactor = 1.0 (gentler falloff) ===\n');
W3 = gradientweight(I, 1.5, 'RolloffFactor', 1.0);
fprintf('W3(4,4) = %.6f (expect 0.768630)\n', W3(4,4));

fprintf('\n=== WeightCutoff = 0.5 (aggressive floor) ===\n');
W4 = gradientweight(I, 1.5, 'WeightCutoff', 0.5);
fprintf('W4(4,4) = %.4g (expect 0.001 — was 0.326 above cutoff)\n', W4(4,4));

fprintf('\n=== constant-image fast-path ===\n');
Wc = gradientweight(ones(5));
fprintf('Wc(3,3) = %g (expect 1)\n', Wc(3,3));
