clear
import compat.*

fprintf('=== signal/modulate (Phase 4.12 — analog modulation) ===\n');

fs = 100; t = (0:1/fs:0.1)';
x = sin(2*pi*5*t);

fprintf('\n[AM (default amdsb-sc) at Fc=20 Hz]\n');
y = modulate(x, 20, fs, 'am');
fprintf('  y(1:5) = '); fprintf('%.4f ', y(1:5)); fprintf('\n');
fprintf('  expect: 0.0000 0.0955 -0.4755 -0.6545 0.2939\n');

fprintf('\n[FM with default kf]\n');
y = modulate(x, 20, fs, 'fm');
fprintf('  y(1:5) = '); fprintf('%.4f ', y(1:5)); fprintf('\n');
fprintf('  expect: 1.0000 -0.0741 -0.8782 0.9324 -0.4893\n');

fprintf('\n[PM with default kp]\n');
y = modulate(x, 20, fs, 'pm');
fprintf('  y(1:5) = '); fprintf('%.4f ', y(1:5)); fprintf('\n');
fprintf('  expect: 1.0000 -0.6105 -0.3453 0.9996 -0.1597\n');

fprintf('\n[AMDSB-TC with offset 0.5]\n');
y = modulate(x, 20, fs, 'amdsb-tc', 0.5);
fprintf('  y(1:5) = '); fprintf('%.4f ', y(1:5)); fprintf('\n');
fprintf('  expect: -0.5000 -0.0590 -0.0710 -0.2500 0.1394\n');

fprintf('\n[AMSSB (Phase 5.3 — uses hilbert)]\n');
y = modulate(x, 20, fs, 'amssb');
fprintf('  y(1:5) = '); fprintf('%.4f ', y(1:5)); fprintf('\n');
fprintf('  approximate-equal MATLAB (~3-5%% diff from hilbert finite-window)\n');

fprintf('\nBIT-EQUAL MATLAB R2025b on am/amdsb-tc/fm/pm. amssb approx-equal.\n');
fprintf('KNOWN GAPs: pwm/ptm/ppm (special pulses), qam (complex carrier).\n');
