clear
import compat.*

% cumtrapz of a COMPLEX y — cumulative trapezoid over both parts (x real).
% Fixed 2026-06-05: previously "complex inputs are not supported".

c = cumtrapz([1+1i 2+2i 3+3i]);
fprintf('cumtrapz([1+1i 2+2i 3+3i]) = %g%+gi %g%+gi %g%+gi  (expect 0, 1.5+1.5i, 4+4i)\n', ...
        real(c(1)),imag(c(1)), real(c(2)),imag(c(2)), real(c(3)),imag(c(3)));

cx = cumtrapz([0 1 3], [1+1i 2+2i 4+4i]);
fprintf('cumtrapz(x,y) last         = %g%+gi  (expect 7.5+7.5i)\n', real(cx(3)), imag(cx(3)));

M = [1+1i 2; 3 4i];
cm = cumtrapz(M);
fprintf('cumtrapz(M) row 2          = %g%+gi, %g%+gi  (expect 2+0.5i, 1+2i)\n', ...
        real(cm(2,1)),imag(cm(2,1)), real(cm(2,2)),imag(cm(2,2)));

cd2 = cumtrapz([1+1i 2+2i 3+3i], 2);
fprintf('cumtrapz(row, 2) last      = %g%+gi  (expect 4+4i)\n', real(cd2(3)), imag(cd2(3)));

r = cumtrapz([1 2 3 4]);
fprintf('cumtrapz([1 2 3 4])(4)     = %g real? %d  (expect 7.5, 1)\n', r(4), isreal(r));
