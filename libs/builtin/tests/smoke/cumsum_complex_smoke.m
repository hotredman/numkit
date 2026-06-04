clear
import compat.*

% cumsum / cumprod on COMPLEX input — accumulate element-wise.
% Fixed 2026-06-05: previously threw "Not a double array".

s = cumsum([1+1i 2+2i 3+3i]);
fprintf('cumsum  = %g%+gi %g%+gi %g%+gi  (expect 1+1i 3+3i 6+6i)\n', ...
        real(s(1)),imag(s(1)), real(s(2)),imag(s(2)), real(s(3)),imag(s(3)));

p = cumprod([1+1i 1-1i 2i]);
fprintf('cumprod = %g%+gi %g%+gi %g%+gi  (expect 1+1i 2+0i 0+4i)\n', ...
        real(p(1)),imag(p(1)), real(p(2)),imag(p(2)), real(p(3)),imag(p(3)));

M  = [1+1i 2; 3 4i];
cs = cumsum(M);
fprintf('cumsum(M) col-wise (2,:) = %g%+gi %g%+gi  (expect 4+1i 2+4i)\n', ...
        real(cs(2,1)),imag(cs(2,1)), real(cs(2,2)),imag(cs(2,2)));

sr = cumsum([1+1i 2+2i 3+3i], 2, 'reverse');
fprintf('cumsum reverse           = %g%+gi %g%+gi %g%+gi  (expect 6+6i 5+5i 3+3i)\n', ...
        real(sr(1)),imag(sr(1)), real(sr(2)),imag(sr(2)), real(sr(3)),imag(sr(3)));
