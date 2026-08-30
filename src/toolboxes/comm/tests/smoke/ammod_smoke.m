clear

fprintf('=== ammod (amplitude modulator) ===\n');

fs = 8000;
fc = 100;
t  = (0:9)' / fs;
x  = sin(2*pi*5*t);

% DSB-SC (default carramp=0)
y = ammod(x, fc, fs);
fprintf('  DSB-SC y(1:5): ');
fprintf('%.6f ', y(1:5));
fprintf('\n  expect 0.000000 0.003915 0.007757 0.011455 0.014939\n');

% DSB-TC with phase + carrier amplitude
y2 = ammod(x, fc, fs, pi/4, 0.5);
fprintf('  DSB-TC carramp=0.5 phase=pi/4 y2(1:5): ');
fprintf('%.6f ', y2(1:5));
fprintf('\n  expect 0.353553 0.327274 0.298509 0.267405 0.234126\n');

% Edge: at t=0, x(1)=0, so DSB-SC gives 0; DSB-TC gives carramp*cos(ini_phase)
fprintf('  y(1) = %.6f (expect 0)\n', y(1));
fprintf('  y2(1) = %.10f (expect %.10f = 0.5*cos(pi/4))\n', y2(1), 0.5 * cos(pi/4));

% Row orientation preserved
xr = x';
yr = ammod(xr, fc, fs);
fprintf('  row input shape: [%d %d] (expect 1 10)\n', size(yr, 1), size(yr, 2));
