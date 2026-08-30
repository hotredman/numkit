clear

fprintf('=== ssbmod (single-sideband modulator) ===\n');

fs = 8000;
fc = 100;
N = 32;
t = (0:N-1)' / fs;
x = sin(2*pi*5*t);

% Lower sideband (default)
yL = ssbmod(x, fc, fs);
fprintf('  LSB y(1:5): ');
fprintf('%.6f ', yL(1:5));
fprintf('\n  expect 0.000000 0.006744 0.013396 0.013840 0.018093\n');

% Upper sideband
yU = ssbmod(x, fc, fs, 0, 'upper');
fprintf('  USB y(1:5): ');
fprintf('%.6f ', yU(1:5));
fprintf('\n  expect 0.000000 0.001086 0.002118 0.009071 0.011784\n');

% LSB with phase shift
yP = ssbmod(x, fc, fs, pi/4);
fprintf('  LSB pi/4 y(1:5): ');
fprintf('%.6f ', yP(1:5));
fprintf('\n  expect 0.081757 0.029970 0.033780 0.014864 0.016227\n');

% Row orientation preserved
xr = x';
yr = ssbmod(xr, fc, fs);
fprintf('  row input shape: [%d %d] (expect 1 %d)\n', size(yr, 1), size(yr, 2), N);
