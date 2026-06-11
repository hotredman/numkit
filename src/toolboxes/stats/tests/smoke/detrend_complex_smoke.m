clear
import compat.*

% detrend of a COMPLEX array — remove the trend from real + imaginary parts.
% Fixed 2026-06-05: previously "Not a double array".

d = detrend([1+2i 5+1i 3-2i 8+0i]);
fprintf('linear d(1,2,4) = %g%+gi %g%+gi %g%+gi  (expect -0.4+0.4i, 1.7+0.3i, 0.9+1.1i)\n', ...
        real(d(1)),imag(d(1)), real(d(2)),imag(d(2)), real(d(4)),imag(d(4)));

dc = detrend([1+2i 5+1i 3-2i 8+0i], 'constant');
fprintf('constant dc(1)  = %g%+gi  (expect -3.25+1.75i, = subtract mean)\n', real(dc(1)), imag(dc(1)));

dr = detrend([1+2i 2+4i 3+6i]);
fprintf('ramp |d|        = %.2g %.2g %.2g  (expect ~0)\n', abs(dr(1)), abs(dr(2)), abs(dr(3)));

M = [1+1i 10; 2 8+2i; 5+1i 6];
dm = detrend(M);
fprintf('matrix dm(1,1)/(2,2) = %g%+gi / %g%+gi  (expect 0.333+0.333i / 0+1.333i)\n', ...
        real(dm(1,1)),imag(dm(1,1)), real(dm(2,2)),imag(dm(2,2)));

r = detrend(2 * (1:5)' + 3);
fprintf('real max|d|     = %.2g  real? %d  (expect ~0, 1)\n', max(abs(r)), isreal(r));
