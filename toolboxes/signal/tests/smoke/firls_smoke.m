clear
import compat.*

% --- Lowpass length 21 ---
b1 = firls(20, [0 0.4 0.5 1], [1 1 0 0]);
fprintf('=== Lowpass (N=20, pass [0,0.4], stop [0.5,1]) ===\n');
fprintf('  length:    %d (expect 21)\n', numel(b1));
fprintf('  symmetric: %d (expect 1)\n', max(abs(b1 - fliplr(b1))) < 1e-12);
fprintf('  b(1):      %.6f (expect ~0.016509)\n', b1(1));
fprintf('  b(11):     %.6f (expect ~0.450390)\n', b1(11));
fprintf('  sum (DC):  %.6f (expect ~1.012)\n', sum(b1));

% --- Bandpass length 41 ---
b2 = firls(40, [0 0.2 0.3 0.6 0.7 1], [0 0 1 1 0 0]);
fprintf('\n=== Bandpass (N=40, pass [0.3,0.6]) ===\n');
fprintf('  length:    %d (expect 41)\n', numel(b2));
fprintf('  symmetric: %d (expect 1)\n', max(abs(b2 - fliplr(b2))) < 1e-12);
fprintf('  peak:      %.6f (expect ~0.397298)\n', b2(21));

% --- Linear ramp in passband ---
b3 = firls(20, [0 0.5 0.6 1], [0 1 0 0]);
fprintf('\n=== Linear-ramp passband (N=20) ===\n');
fprintf('  length:    %d (expect 21)\n', numel(b3));
fprintf('  symmetric: %d (expect 1)\n', max(abs(b3 - fliplr(b3))) < 1e-12);
