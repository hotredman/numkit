clear
import compat.*

% trapz of a COMPLEX y — trapezoidal sum over both parts (x-spacing real).
% Fixed 2026-06-05: previously "Not a double array". (cumtrapz still pending.)

t = trapz([1+1i 2+2i 3+3i]);
fprintf('trapz([1+1i 2+2i 3+3i]) = %g%+gi   (expect 4+4i)\n', real(t), imag(t));

tx = trapz([0 1 2], [1+1i 2+2i 5+5i]);
fprintf('trapz([0 1 2],[...])    = %g%+gi   (expect 5+5i)\n', real(tx), imag(tx));

M = [1+1i 2; 3 4i];
tm = trapz(M);
fprintf('trapz(M) col-wise       = %g%+gi, %g%+gi   (expect 2+0.5i, 1+2i)\n', ...
        real(tm(1)),imag(tm(1)), real(tm(2)),imag(tm(2)));

td = trapz([1+1i 2+2i 3+3i], 2);
fprintf('trapz(row, 2)           = %g%+gi   (expect 4+4i)\n', real(td), imag(td));

fprintf('trapz([1 2 3 4]) real   = %g real? %d   (expect 7.5, 1)\n', trapz([1 2 3 4]), isreal(trapz([1 2 3 4])));
