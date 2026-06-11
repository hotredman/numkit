clear
import compat.*

% diff() on a COMPLEX array — real + imaginary differenced together.
% Fixed 2026-06-05: previously the imaginary part was silently dropped.

d = diff([1+2i 4+6i 9+12i]);
fprintf('diff([1+2i 4+6i 9+12i]) = %g%+gi, %g%+gi  (expect 3+4i, 5+6i)\n', ...
        real(d(1)), imag(d(1)), real(d(2)), imag(d(2)));

d2 = diff([1+2i 4+6i 9+12i 16+20i], 2);
fprintf('2nd order               = %g%+gi, %g%+gi  (expect 2+2i, 2+2i)\n', ...
        real(d2(1)), imag(d2(1)), real(d2(2)), imag(d2(2)));

M  = [1+1i 2+2i; 5+1i 6+3i];
dm = diff(M, 1, 2);
fprintf('diff(M,1,2)             = %g%+gi; %g%+gi  (expect 1+1i; 1+2i)\n', ...
        real(dm(1)), imag(dm(1)), real(dm(2)), imag(dm(2)));

d0 = diff([2+3i 7+1i], 0);
fprintf('diff(...,0) identity    = %g%+gi, %g%+gi  (expect 2+3i, 7+1i)\n', ...
        real(d0(1)), imag(d0(1)), real(d0(2)), imag(d0(2)));
