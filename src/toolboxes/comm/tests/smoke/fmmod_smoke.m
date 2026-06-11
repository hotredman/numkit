clear
import compat.*

fprintf('=== fmmod (frequency modulator) ===\n');

fs = 8000;
fc = 100;
t  = (0:9)' / fs;
x  = sin(2*pi*5*t);
freqdev = 50;

y = fmmod(x, fc, fs, freqdev);
fprintf('  fmmod default: ');
fprintf('%.6f ', y);
fprintf('\n  expect 1.000000 0.996905 0.987616 0.972154 0.950579 0.922992 0.889532 0.850376 0.805742 0.755881\n');

y2 = fmmod(x, fc, fs, freqdev, pi/3);
fprintf('  fmmod ini=pi/3: ');
fprintf('%.6f ', y2);
fprintf('\n  expect 0.500000 0.430372 0.357936 0.283128 0.206403 0.128233 0.049102 -0.030493 -0.110048 -0.189054\n');

% At t=0, x=0: cumsum=0, so y(1) = cos(2*pi*0 + 0 + 0) = 1
fprintf('  y(1) at t=0 = %.6f (expect 1)\n', y(1));

% Row orientation preserved
xr = x';
yr = fmmod(xr, fc, fs, freqdev);
fprintf('  row input shape: [%d %d] (expect 1 10)\n', size(yr, 1), size(yr, 2));
