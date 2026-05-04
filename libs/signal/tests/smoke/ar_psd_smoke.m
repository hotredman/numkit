clear

import compat.*

% --- Synthesise an AR(2) process and try to recover its peak ---
% AR(2) with poles at 0.95·exp(±j·π/4) → resonant peak at ω = π/4 ≈ 0.785
rng(42);
N = 1024;
% Drive with WGN, IIR filter via simple recursion.
a1 = -2 * 0.95 * cos(pi/4);   % first AR coefficient (with positive sign in y[n] − a1·y[n−1] − a2·y[n−2] = e[n])
a2 = 0.95^2;
e = randn(N + 100, 1);
y = zeros(N + 100, 1);
for n = 3:numel(y)
    y(n) = -a1 * y(n-1) - a2 * y(n-2) + e(n);
end
x = y(101:end);   % drop transient

% --- pyulear with order 2 should put a strong peak at ω ≈ π/4 ---
[Pxx_yw, F] = pyulear(x, 2);
[~, k_yw] = max(Pxx_yw);
peak_yw = F(k_yw);
fprintf('--- pyulear order=2 on AR(2) with peak at π/4 ≈ 0.785 ---\n');
fprintf('  peak at ω = %.4f rad (expect ≈ 0.785)\n', peak_yw);
fprintf('  numel(Pxx) = %d\n\n', numel(Pxx_yw));

% --- pburg on the same: similar peak location ---
[Pxx_bg, F] = pburg(x, 2);
[~, k_bg] = max(Pxx_bg);
peak_bg = F(k_bg);
fprintf('--- pburg order=2 ---\n');
fprintf('  peak at ω = %.4f rad (expect ≈ 0.785)\n', peak_bg);

% --- Energy match between pburg & pyulear (very close on stationary AR) ---
fprintf('\n--- relative energy difference between methods ---\n');
fprintf('  sum(Pxx_yw) = %.4f, sum(Pxx_bg) = %.4f\n', ...
    sum(Pxx_yw), sum(Pxx_bg));
fprintf('  ratio bg / yw = %.4f (expect ≈ 1)\n\n', sum(Pxx_bg) / sum(Pxx_yw));

% --- Higher-order pyulear on a pure tone: AR(4) on sin ⇒ sharp peak ---
n = (0:1023)';
omega0 = 0.5;
sig = cos(omega0 * n) + 0.05 * randn(numel(n), 1);
[P_t, F_t] = pyulear(sig, 4);
[~, k_t] = max(P_t);
peak_t = F_t(k_t);
fprintf('--- pyulear order=4 on cos(0.5·n)+small noise ---\n');
fprintf('  peak at ω = %.4f (expect ≈ 0.5000)\n', peak_t);
fprintf('  Pxx(peak) / mean(Pxx) = %.1f (expect ≫ 1, sharp resonance)\n', ...
    P_t(k_t) / mean(P_t));

% --- DC: AR PSD should not blow up at f=0 ---
fprintf('\n--- DC behaviour ---\n');
fprintf('  Pxx_yw(1) = %.4f (finite — no blow-up)\n', Pxx_yw(1));
fprintf('  Pxx_bg(1) = %.4f (finite)\n', Pxx_bg(1));
