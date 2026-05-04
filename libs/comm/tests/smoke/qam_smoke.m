clear

import compat.*

% PAM round-trip
data = [0 1 2 3];
s = pammod(data, 4);
fprintf('--- pammod([0 1 2 3], M=4) ---\n');
disp(real(s));
fprintf('  expect: gray-coded amplitudes [-3 -1 3 1]\n\n');

out = pamdemod(s, 4);
fprintf('round-trip: '); disp(out');
fprintf('  expect: [0 1 2 3]\n\n');

% 16-QAM round-trip
data16 = 0:15;
s16 = qammod(data16, 16);
fprintf('--- qammod(0:15, M=16) ---\n');
disp(s16);
fprintf('  expect: 16 distinct points on a 4×4 Gray-coded grid\n\n');

out16 = qamdemod(s16, 16);
fprintf('--- qamdemod round-trip ---\n');
disp(out16');
fprintf('  expect: [0 1 ... 15]\n\n');

% QAM with unit average power
s16u = qammod(data16, 16, 'gray', 'UnitAveragePower', true);
avg_pow = mean(abs(s16u).^2);
fprintf('--- qammod with UnitAveragePower=true ---\n');
fprintf('avg power = %.4f (expect ≈ 1.0)\n\n', avg_pow);

out16u = qamdemod(s16u, 16, 'gray', 'UnitAveragePower', true);
fprintf('UnitPower round-trip: '); disp(out16u');
fprintf('  expect: [0 1 ... 15]\n\n');

% modnorm: scale a constellation to unit avg power
ref = qammod(0:15, 16);
sc = modnorm(ref, 'avpow', 1);
fprintf('--- modnorm (avpow=1) ---\n');
fprintf('scale = %.6f, after-scale avg power = %.4f\n', ...
    sc, mean(abs(ref * sc).^2));
fprintf('  expect: ≈ 1.0\n\n');

% modnorm peakpow
sc2 = modnorm(ref, 'peakpow', 1);
fprintf('--- modnorm (peakpow=1) ---\n');
fprintf('scale = %.6f, after-scale peak power = %.4f\n', ...
    sc2, max(abs(ref * sc2).^2));
fprintf('  expect: ≈ 1.0\n');
