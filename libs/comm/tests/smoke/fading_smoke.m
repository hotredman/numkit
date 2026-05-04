clear

import compat.*

% --- Rayleigh: E[|h|²] = 1, samples large enough for stable mean ---
rng(42);
N = 100000;
x = ones(N, 1);             % unit-power input → output power = |h|²
y = rayleighchan(x);
power = mean(abs(y).^2);
fprintf('--- rayleighchan, N=%d unit input ---\n', N);
fprintf('  mean(|y|²) = %.4f (expect ~ 1.0)\n', power);
fprintf('  std(|y|²)  = %.4f (expect ~ 1.0 for exponential)\n\n', std(abs(y).^2));

% --- Rayleigh: phase distribution should be ~ uniform over [-pi, pi] ---
phases = angle(y);
fprintf('  mean(phase)  = %.4f (expect ~ 0)\n', mean(phases));
fprintf('  std(phase)   = %.4f (expect ~ pi/sqrt(3) = 1.8138)\n\n', std(phases));

% --- Rician with K=10: most energy in LOS, low fade variance ---
y2 = ricianchan(x, 10);
p2 = mean(abs(y2).^2);
fprintf('--- ricianchan(K=10) ---\n');
fprintf('  mean(|y|²) = %.4f (expect ~ 1.0 — Rician total power is unit)\n', p2);
fprintf('  std(|y|²)  = %.4f (expect << Rayleigh std due to high K)\n\n', ...
    std(abs(y2).^2));

% --- Rician with K=0 ⇒ degenerates to Rayleigh ---
y3 = ricianchan(x, 0);
p3 = mean(abs(y3).^2);
fprintf('--- ricianchan(K=0) ≡ Rayleigh ---\n');
fprintf('  mean(|y|²) = %.4f (expect ~ 1.0)\n', p3);
fprintf('  std(|y|²)  = %.4f (expect ~ 1.0)\n\n', std(abs(y3).^2));

% --- Rician with K → ∞ ⇒ degenerates to AWGN-free LOS, |h| ≈ 1 ---
y4 = ricianchan(x, 1000);
p4 = mean(abs(y4).^2);
fprintf('--- ricianchan(K=1000) — strong LOS ---\n');
fprintf('  mean(|y|²) = %.4f (expect ~ 1.0)\n', p4);
fprintf('  std(|y|²)  = %.4f (expect very small)\n\n', std(abs(y4).^2));

% --- Composes with modulator + AWGN ---
% Generate BPSK, fade, add noise.
data = randi([0 1], N, 1);
sym  = pskmod(data, 2);    % BPSK
faded = rayleighchan(sym);
noisy = awgn(faded, 10);
fprintf('--- BPSK → rayleighchan → awgn(10 dB) ---\n');
fprintf('  size(noisy) = %dx%d\n', size(noisy, 1), size(noisy, 2));
fprintf('  mean(|noisy|²) ≈ %.4f (expect ~ 1.0 + noise contribution)\n', ...
    mean(abs(noisy).^2));
