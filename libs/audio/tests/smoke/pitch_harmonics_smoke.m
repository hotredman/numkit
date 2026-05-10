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

fprintf('\n[pitch PEF method on 220 Hz sine — Cycle K-2]\n');
f0_pef = pitch(x, fs, 'Method', 'PEF');
fprintf('  PEF first 5: '); fprintf('%.6f ', f0_pef(1:5)); fprintf('\n');
fprintf('  expect: 220.604203 (constant across frames — bit-equal MATLAB)\n');
fprintf('  PEF mean = %.6f (expect 220.604203)\n', mean(f0_pef));

fprintf('\n[pitch Range NV arg — Cycle L (partial)]\n');
xmix = sin(2*pi*220*t) + 0.5*sin(2*pi*100*t);
f_rH = pitch(xmix, fs, 'Range', [150 400]);
f_rL = pitch(xmix, fs, 'Range', [50 150]);
f_rPEF = pitch(xmix, fs, 'Method', 'PEF', 'Range', [80 250]);
fprintf('  Range [150 400]: f0(1)=%.4f mean=%.4f (picks 220 dominantly)\n', f_rH(1), mean(f_rH));
fprintf('  Range [50 150]:  f0(1)=%.4f mean=%.4f (picks 100 / subharmonic)\n', f_rL(1), mean(f_rL));
fprintf('  PEF Range [80 250]: f0(1)=%.4f (expect 221.2508 bit-equal MATLAB)\n', f_rPEF(1));

fprintf('\nKNOWN GAPs:\n');
fprintf('  pitch methods: NCF (default) + CEP (cycle K) + PEF (cycle K-2) shipped.\n');
fprintf('  CEP and PEF are bit-equal MATLAB R2025b.\n');
fprintf('  LHS/SRH still deferred — block on libs/signal::fft non-power-of-2 fix\n');
fprintf('    (both methods use fftLength=round(fs) which is non-power-of-2 typically).\n');
fprintf('  pitchnn: requires DNN runtime, deferred entirely.\n');
fprintf('  harmonicRatio: full MATLAB R2025b parity (auto low-edge + parabolic refinement).\n');
