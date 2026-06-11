clear
import compat.*

% gradient of a COMPLEX array — real + imaginary parts gradiented separately.
% Fixed 2026-06-05: previously "complex inputs are not supported".
% (N-D complex is still 1-D/2-D only — see bugs/builtin/gradient-3d.md.)

g = gradient([1+1i 3+3i 5+5i]);
fprintf('gradient([1+1i 3+3i 5+5i]) = %g%+gi %g%+gi %g%+gi  (expect 2+2i x3)\n', ...
        real(g(1)),imag(g(1)), real(g(2)),imag(g(2)), real(g(3)),imag(g(3)));

gm = gradient([1+2i 3 5-1i]);
fprintf('gradient([1+2i 3 5-1i])    = %g%+gi %g%+gi %g%+gi  (expect 2-2i, 2-1.5i, 2-1i)\n', ...
        real(gm(1)),imag(gm(1)), real(gm(2)),imag(gm(2)), real(gm(3)),imag(gm(3)));

M  = [1+1i 2 4; 3+1i 4i 6];
gx = gradient(M);
fprintf('gradient(M) row1 (x-grad)  = %g%+gi %g%+gi  (expect 1-1i, 1.5-0.5i)\n', ...
        real(gx(1,1)),imag(gx(1,1)), real(gx(1,2)),imag(gx(1,2)));

[fx, fy] = gradient(M);
fprintf('[fx,fy]: fy(1,1) (y-grad)   = %g%+gi  (expect 2+0i)\n', real(fy(1,1)), imag(fy(1,1)));

r = gradient([1 4 9]);
fprintf('real gradient([1 4 9])      = %g %g %g  real? %d  (expect 3 4 5, 1)\n', ...
        r(1), r(2), r(3), isreal(r));
