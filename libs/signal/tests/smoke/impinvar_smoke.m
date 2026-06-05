clear

import compat.*

% Single real pole: H(s) = 1 / (s + 1)
% Sample at fs = 10. T = 0.1.
%   α = exp(-T) = 0.9048
%   r = 1 / a'(p) = 1 / 1 = 1
%   H_d(z) = T·1 / (1 - α·z⁻¹) = 0.1 / (1 - 0.9048·z⁻¹)
[bd, ad] = impinvar([1], [1, 1], 10);
fprintf('Test 1 — H(s) = 1/(s+1), fs = 10:\n');
fprintf('  bd = ['); for i = 1:length(bd); fprintf('%.8f ', bd(i)); end; fprintf(']\n');
fprintf('  ad = ['); for i = 1:length(ad); fprintf('%.8f ', ad(i)); end; fprintf(']\n');
fprintf('  MATLAB R2025b: bd = [0.10000000], ad = [1, -0.90483742]\n');

% Two-pole real-coef analog filter: H(s) = 1 / (s² + 3s + 2) = 1 / ((s+1)(s+2))
% Poles at -1 and -2.
% r1 = 1/(p1 - p2) = 1/(-1 - (-2)) = 1
% r2 = 1/(p2 - p1) = 1/(-2 - (-1)) = -1
% At fs = 10, T = 0.1:
%   α1 = exp(-0.1) = 0.9048
%   α2 = exp(-0.2) = 0.8187
%   H_d(z) = T · (1/(1 - α1·z⁻¹) - 1/(1 - α2·z⁻¹))
%   a_d = (1 - α1·z⁻¹)(1 - α2·z⁻¹) = 1 - 1.7236·z⁻¹ + 0.7408·z⁻²
%   b_d = T · ((1 - α2·z⁻¹) - (1 - α1·z⁻¹)) = T · (α1 - α2)·z⁻¹
%       = 0.1 · 0.0861 · z⁻¹ = 0.008611 · z⁻¹
%   So bd = [0, 0.00861] (length 2)
[bd2, ad2] = impinvar([1], [1, 3, 2], 10);
fprintf('Test 2 — H(s) = 1/((s+1)(s+2)), fs = 10:\n');
fprintf('  bd = ['); for i = 1:length(bd2); fprintf('%.8f ', bd2(i)); end; fprintf(']\n');
fprintf('  ad = ['); for i = 1:length(ad2); fprintf('%.8f ', ad2(i)); end; fprintf(']\n');
fprintf('  MATLAB R2025b: bd = [0, 0.00861067], ad = [1, -1.72356817, 0.74081822]\n');

% Round-trip test: sum bd / sum ad at z=1 should match analog H(0) ≈ 1/2 (for 1/(s²+3s+2))
fprintf('  H_d(1) = %.6f  (analog H(0) = 1/2 = 0.5; impinvar approximates at low freq)\n', ...
    sum(bd2) / sum(ad2));

% REPEATED poles (bugs/signal/impinvar-repeated-poles.md — was wrong, fixed 2026-06-05).
fprintf('\nTest 3 — double pole 1/(s+1)^2, fs = 10:\n');
[bz, az] = impinvar(1, [1 2 1], 10);
fprintf('  bz = ['); for i=1:numel(bz); fprintf('%.10g ', bz(i)); end; fprintf(']\n');
fprintf('  az = ['); for i=1:numel(az); fprintf('%.10g ', az(i)); end; fprintf(']\n');
fprintf('  MATLAB: bz = [0 0.00904837418], az = [1 -1.809674836 0.8187307531]\n');

fprintf('\nTest 4 — triple pole 1/(s+1)^3, fs = 10:\n');
[bz, az] = impinvar(1, [1 3 3 1], 10);
fprintf('  bz = ['); for i=1:numel(bz); fprintf('%.10g ', bz(i)); end; fprintf(']\n');
fprintf('  MATLAB: bz = [0 0.000452418709 0.0004093653765]\n');

fprintf('\nTest 5 — mixed [1 2]/((s+1)^2(s+2)), fs = 10:\n');
[bz, az] = impinvar([1 2], [1 4 5 2], 10);
fprintf('  bz = ['); for i=1:numel(bz); fprintf('%.10g ', bz(i)); end; fprintf(']\n');
fprintf('  MATLAB: bz = [0 0.00904837418 -0.007408182207]\n');
