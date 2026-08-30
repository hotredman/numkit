clear

fprintf('=== pmmod (phase modulator) ===\n');

fs = 8000;
fc = 100;
t  = (0:9)' / fs;
x  = sin(2*pi*5*t);
phasedev = pi/4;

y = pmmod(x, fc, fs, phasedev);
fprintf('  pmmod (default ini_phase=0): ');
fprintf('%.6f ', y);
fprintf('\n  expect 1.000000 0.996671 0.986705 0.970168 0.947172 0.917869 0.882454 0.841163 0.794272 0.742092\n');

y2 = pmmod(x, fc, fs, phasedev, pi/3);
fprintf('  pmmod (ini_phase=pi/3): ');
fprintf('%.6f ', y2);
fprintf('\n  expect 0.500000 0.427725 0.352602 0.275132 0.195829 0.115223 0.033850 -0.047749 -0.129029 -0.209450\n');

% Row vs column orientation preserved
xr = x';                 % row vector
yr = pmmod(xr, fc, fs, phasedev);
fprintf('  row input shape: [%d %d] (expect 1 10)\n', size(yr, 1), size(yr, 2));

% At t=0: should be cos(phasedev*x(1) + ini_phase). x(1) = sin(0) = 0.
% So y(1) = cos(0) = 1 (ini_phase=0)
fprintf('  y(1) at t=0 with x(1)=0: %.6f (expect 1.000000)\n', y(1));
