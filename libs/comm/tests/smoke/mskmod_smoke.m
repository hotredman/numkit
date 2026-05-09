clear
import compat.*

fprintf('=== mskmod (minimum-shift keying, differential variant) ===\n');

x = [1 0 1 1 0 0 1 0]';
nSamp = 4;
y = mskmod(x, nSamp);
fprintf('  input bits: ');
fprintf('%d ', x);
fprintf('\n  output length = %d (expect %d * %d = %d)\n', ...
        length(y), length(x), nSamp, length(x) * nSamp);

fprintf('  y(1)  = %+.6f%+.6fi  (expect +1+0i, start of unit circle)\n', real(y(1)), imag(y(1)));
fprintf('  y(2)  = %+.6f%+.6fi  (expect +0.923880+0.382683i = exp(i*pi/8))\n', real(y(2)), imag(y(2)));
fprintf('  y(5)  = %+.6f%+.6fi  (expect +0+1i = exp(i*pi/2), end of first symbol)\n', real(y(5)), imag(y(5)));
fprintf('  y(17) = %+.6f%+.6fi  (expect -1+0i = exp(i*pi))\n', real(y(17)), imag(y(17)));

% All on unit circle
err = max(abs(abs(y) - 1));
fprintf('  max |abs(y) - 1| = %g (expect ~0, all on unit circle)\n', err);

% Row input -> row output
xr = x';
yr = mskmod(xr, nSamp);
fprintf('  row input shape: [%d %d] (expect [1 %d])\n', size(yr, 1), size(yr, 2), length(x) * nSamp);

% ini_phase rotation
yp = mskmod(x, nSamp, pi/4);
fprintf('  with ini_phase=pi/4: y(1) = %+.6f%+.6fi (expect cos(pi/4)+i*sin(pi/4))\n', real(yp(1)), imag(yp(1)));
