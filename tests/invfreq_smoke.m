import compat.*

% --- invfreqs: round-trip a known analog filter ---
% True system: H(s) = 1 / (s² + 0.6s + 1)
% Sample its response on a grid, then refit:
w = linspace(0.1, 5, 50);
H_true = 1 ./ ((1i*w).^2 + 0.6*(1i*w) + 1);

[b, a] = invfreqs(H_true, w, 0, 2);
fprintf('invfreqs (true H = 1/(s²+0.6s+1)):\n');
fprintf('  b = ['); for i = 1:length(b); fprintf('%.6f ', b(i)); end; fprintf(']\n');
fprintf('  a = ['); for i = 1:length(a); fprintf('%.6f ', a(i)); end; fprintf(']\n');
fprintf('  expect b ≈ [1], a ≈ [1, 0.6, 1]\n');

% --- invfreqz: same idea for digital ---
% True: H(z) = 0.0675 / (1 - 1.143·z⁻¹ + 0.4128·z⁻²)  (some 2-pole IIR)
b_true = [0.0675];
a_true = [1, -1.143, 0.4128];
w2 = linspace(0.05, pi-0.1, 64);
% Evaluate H at each w using polynomial-eval at z = exp(jw)
H2 = zeros(1, length(w2));
for k = 1:length(w2)
    z = exp(1i * w2(k));
    num = 0.0675;
    den = 1 - 1.143 * z^(-1) + 0.4128 * z^(-2);
    H2(k) = num / den;
end

[bz, az] = invfreqz(H2, w2, 0, 2);
fprintf('invfreqz (true H = 0.0675/(1 - 1.143z⁻¹ + 0.4128z⁻²)):\n');
fprintf('  b = ['); for i = 1:length(bz); fprintf('%.6f ', bz(i)); end; fprintf(']\n');
fprintf('  a = ['); for i = 1:length(az); fprintf('%.6f ', az(i)); end; fprintf(']\n');
fprintf('  expect b ≈ [0.0675], a ≈ [1, -1.143, 0.4128]\n');
