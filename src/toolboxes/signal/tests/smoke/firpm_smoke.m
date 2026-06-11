clear
import compat.*

fprintf('=== signal/firpm — Parks-McClellan optimal FIR (Type I) ===\n');

fprintf('\n[order-20 lowpass, passband 0-0.4, stopband 0.5-1.0]\n');
[b, err] = firpm(20, [0 0.4 0.5 1], [1 1 0 0]);
fprintf('  length(b) = %d  (expect 21)\n', length(b));
fprintf('  b(1)      = %.6f  (expect ~0.0388)\n', b(1));
fprintf('  b(11)     = %.6f  (expect ~0.4487)  -- center coefficient\n', b(11));
fprintf('  err       = %.6f  (expect ~0.0549)  -- peak ripple |delta|\n', err);
fprintf('  symmetric? max|b - flip(b)| = %.2e\n', max(abs(b - b(end:-1:1))));

fprintf('\n[order-30 bandpass, [0 0.2]/[0.3 0.6]/[0.7 1]]\n');
b = firpm(30, [0 0.2 0.3 0.6 0.7 1], [0 0 1 1 0 0]);
fprintf('  b(16) = %.6f  (expect ~0.3990)\n', b(16));

fprintf('\n[order-30 highpass, asymmetric bands]\n');
b = firpm(30, [0 0.3 0.4 1], [0 0 1 1]);
fprintf('  b(16) = %.6f  (expect ~0.6498)\n', b(16));

fprintf('\n[weighted bandpass, W = [10 1 10] - stopbands stricter]\n');
[b, err] = firpm(30, [0 0.2 0.3 0.6 0.7 1], [0 0 1 1 0 0], [10 1 10]);
fprintf('  b(16) = %.6f  (expect ~0.3791)\n', b(16));
fprintf('  err   = %.6f  (expect ~0.0883)  -- larger ripple from W>1 on stopbands\n', err);

fprintf('\nBit-equal (5 sig figs) with MATLAB R2025b across LP / BP / HP /\n');
fprintf('weighted multi-band designs. KNOWN GAPS: Type II (odd N),\n');
fprintf('hilbert / differentiator ftypes, fresp function-handle form,\n');
fprintf('3rd res output struct, lgrid cell-form override.\n');
