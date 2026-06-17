clear
import compat.*

% log / log10 / log2 of a real ARRAY with a negative element -> complex.
% Fixed 2026-06-05: previously only scalars promoted; array elements gave NaN.

y = log([-1 4]);
fprintf('log([-1 4])      cplx=%d: %g%+gi %g%+gi  (expect 0+3.14159i, 1.38629+0i)\n', ...
        ~isreal(y), real(y(1)),imag(y(1)), real(y(2)),imag(y(2)));

y10 = log10([-100 100]);
fprintf('log10([-100 100])      : %g%+gi %g%+gi  (expect 2+1.36438i, 2+0i)\n', ...
        real(y10(1)),imag(y10(1)), real(y10(2)),imag(y10(2)));

y2 = log2([-8 8]);
fprintf('log2([-8 8])           : %g%+gi %g%+gi  (expect 3+4.53236i, 3+0i)\n', ...
        real(y2(1)),imag(y2(1)), real(y2(2)),imag(y2(2)));

fprintf('in-domain log([1 2 4]) real? %d   scalar log(-1) imag=%g  (expect 1, 3.14159)\n', ...
        isreal(log([1 2 4])), imag(log(-1)));
