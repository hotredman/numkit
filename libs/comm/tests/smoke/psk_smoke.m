clear

import compat.*

% PSK round-trip with M=4 (QPSK)
M = 4;
data = [0 1 2 3 0 2 1 3];

s = pskmod(data, M);
fprintf('--- pskmod([0 1 2 3 0 2 1 3], M=4) ---\n');
disp(s);
fprintf('  expect: 8 unit-magnitude complex points\n\n');

% Round-trip
out = pskdemod(s, M);
fprintf('--- pskdemod round-trip ---\n');
disp(out');
fprintf('  expect: [0 1 2 3 0 2 1 3] (matches input)\n\n');

% PSK with explicit binary order
s2 = pskmod(data, M, 0, 'bin');
out2 = pskdemod(s2, M, 0, 'bin');
fprintf('--- pskmod / pskdemod with bin order ---\n');
disp(out2');
fprintf('  expect: [0 1 2 3 0 2 1 3]\n\n');

% Add noise and check robustness (small SNR margin)
rng(42);
y = s + 0.1 * (randn(size(s)) + 1i*randn(size(s)));
out_noisy = pskdemod(y, M);
errs = sum(out_noisy(:) ~= data(:));
fprintf('--- pskdemod with noise (σ=0.1) ---\n');
fprintf('errors: %d / %d\n', errs, length(data));
fprintf('  expect: 0 errors at low noise\n\n');

% DPSK round-trip
s_d = dpskmod(data, M);
out_d = dpskdemod(s_d, M);
fprintf('--- dpskmod / dpskdemod round-trip ---\n');
disp(out_d');
fprintf('  expect: [0 1 2 3 0 2 1 3]\n\n');

% BPSK (M=2)
data2 = [0 1 1 0 1 0 0 1];
s3 = pskmod(data2, 2);
out3 = pskdemod(s3, 2);
fprintf('--- pskmod / pskdemod BPSK (M=2) ---\n');
disp(out3');
fprintf('  expect: [0 1 1 0 1 0 0 1]\n');
