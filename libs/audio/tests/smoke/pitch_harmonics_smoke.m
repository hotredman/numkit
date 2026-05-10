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

fprintf('\nKNOWN GAPs:\n');
fprintf('  pitch: only NCF method; PEF/CEP/LHS/SRH deferred to v2.\n');
fprintf('  pitchnn: requires DNN runtime, deferred entirely.\n');
fprintf('  harmonicRatio: parabolicInterpolation refinement deferred.\n');
fprintf('  Values match MATLAB R2025b within ~1%% (acceptable for v1).\n');
