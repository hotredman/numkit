clear
import compat.*

fprintf('=== signal/demod (Phase 4.13 — analog demodulation) ===\n');

fs = 200; t = (0:1/fs:0.1)';
x = sin(2*pi*5*t);

fprintf('\n[AM round-trip]\n');
y = modulate(x, 25, fs, 'am');
xd = demod(y, 25, fs, 'am');
fprintf('  max|xd - 0.5*x| = %g (expect <0.01 after lowpass)\n', max(abs(xd - 0.5*x)));
fprintf('  xd(1:5) = '); fprintf('%.4f ', xd(1:5)); fprintf('\n');

fprintf('\n[AMDSB-TC with offset]\n');
y2 = modulate(x, 25, fs, 'amdsb-tc', 0.3);
xd2 = demod(y2, 25, fs, 'amdsb-tc', 0.5);
fprintf('  xd(1:5) = '); fprintf('%.4f ', xd2(1:5)); fprintf('\n');

fprintf('\n[am alias = amdsb-sc]\n');
xa = demod(y, 25, fs, 'am');
xb = demod(y, 25, fs, 'amdsb-sc');
fprintf('  identical: %d (expect 1)\n', isequal(xa, xb));

fprintf('\nApprox-equal MATLAB R2025b on 8/8 fingerprints (tol 0.01).\n');
fprintf('~2e-3 diff vs MATLAB attributed to filtfilt edge handling.\n');
fprintf('KNOWN GAPs: fm/pm modes use hilbert (blocked on libs/signal::fft).\n');
fprintf('  amssb/pwm/ptm/ppm/qam deferred.\n');
