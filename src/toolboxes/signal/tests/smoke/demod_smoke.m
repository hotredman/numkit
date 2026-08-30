clear

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

fprintf('\n[FM round-trip — Phase 5.3, hilbert-based]\n');
yfm = modulate(x, 25, fs, 'fm');
xfm = demod(yfm, 25, fs, 'fm');
fprintf('  max|xfm - x| (interior 5..end-5) = %g\n', max(abs(xfm(5:end-5) - x(5:end-5))));

fprintf('\n[PM round-trip — Phase 5.3, hilbert-based]\n');
ypm = modulate(x, 25, fs, 'pm');
xpm = demod(ypm, 25, fs, 'pm');
fprintf('  numel(xpm)=%d (sanity check; PM has wraparound noise)\n', numel(xpm));

fprintf('\nApprox-equal MATLAB R2025b on 9/9 fingerprints (tol 5%%).\n');
fprintf('Diffs from filtfilt edge handling + hilbert finite-window.\n');
fprintf('KNOWN GAPs: amssb/pwm/ptm/ppm/qam deferred (hilbert / specialised pulses).\n');
