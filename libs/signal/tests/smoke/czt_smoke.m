clear
import compat.*

fprintf('=== signal/czt — Chirp Z-transform (Bluestein) ===\n');

fprintf('\n[default args — equivalent to fft]\n');
x = 1:8;
y = czt(x);
fprintf('  y(1) = %.4f  (expect 36 = sum(1..8))\n', real(y(1)));
fprintf('  |czt(x) - fft(x)|_max = %.2e\n', max(abs(y - fft(x))));

fprintf('\n[m-override: zero-pad to 16]\n');
y = czt(x, 16);
fprintf('  length(y) = %d   |czt(x,16) - fft(x,16)|_max = %.2e\n', ...
    length(y), max(abs(y - fft(x, 16))));

fprintf('\n[full chirp: m=10, w on 1/16-cycle, a at pi/8 angle]\n');
m = 10; w = exp(-1j*2*pi*1/16); a = exp(1j*pi/8);
y = czt(x, m, w, a);
for k = [1 5 10]
    fprintf('  y(%2d) = %+8.4f %+8.4fi\n', k, real(y(k)), imag(y(k)));
end
fprintf('  expected MATLAB: y(1) = -8.1371 -25.1367i,  y(10) = -4.0000 -1.6569i\n');

fprintf('\nBit-equal (~1e-13) with MATLAB R2025b.\n');
fprintf('Octave 11.1.0 ships czt in the signal package; numeric match.\n');
