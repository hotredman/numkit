clear
import compat.*

fprintf('=== Audio Cycle E — pitch + harmonicRatio (FINAL audio cycle) ===\n');

fs = 16000;
t = (0:1/fs:1)';

fprintf('\n[pitch on 220 Hz sine]\n');
x = sin(2*pi*220*t);
f0 = pitch(x, fs);
fprintf('  numel=%d (expect 95)\n', numel(f0));
fprintf('  first=%.4f mean=%.4f (expect ~220)\n', f0(1), mean(f0));

fprintf('\n[pitch on 100 Hz sine]\n');
y = sin(2*pi*100*t);
f0_low = pitch(y, fs);
fprintf('  first=%.4f mean=%.4f (expect ~100)\n', f0_low(1), mean(f0_low));

fprintf('\n[harmonicRatio on pure sine]\n');
hr = harmonicRatio(x, fs);
fprintf('  numel=%d (expect 98)\n', numel(hr));
fprintf('  mean=%.6f (expect close to 1 for pure tone)\n', mean(hr));

fprintf('\n[harmonicRatio on noise — should be low]\n');
rng(42);
n = randn(fs, 1);
hr_n = harmonicRatio(n, fs);
fprintf('  mean=%.6f (expect close to 0)\n', mean(hr_n));

fprintf('\n[pitch CEP method on 220 Hz sine — Cycle K]\n');
f0_cep = pitch(x, fs, 'Method', 'CEP');
fprintf('  CEP first 5: '); fprintf('%.4f ', f0_cep(1:5)); fprintf('\n');
fprintf('  expect: 213.3333 246.1538 246.1538 216.2162 246.1538 (bit-equal MATLAB)\n');
fprintf('  CEP mean = %.4f (expect 233.6022)\n', mean(f0_cep));

fprintf('\nKNOWN GAPs:\n');
fprintf('  pitch methods: NCF (default) + CEP (cycle K) shipped — bit-equal MATLAB.\n');
fprintf('  PEF/LHS/SRH still deferred to v2.\n');
fprintf('  pitchnn: requires DNN runtime, deferred entirely.\n');
fprintf('  harmonicRatio: full MATLAB R2025b parity (auto low-edge + parabolic refinement).\n');
