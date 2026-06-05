clear
import compat.*

% filter of COMPLEX signals/taps — genuine complex IIR/FIR recurrence (BILINEAR).
% Fixed 2026-06-05: previously "Not a double array". Closes the complex-input
% umbrella (last of trapz/cumtrapz/median/interp1/gradient/movmean/detrend/conv).

y = filter([1 1], 1, [1i 1i 1i]);
fprintf('FIR  = %g%+gi %g%+gi %g%+gi  (expect 1i 2i 2i)\n', ...
        real(y(1)),imag(y(1)), real(y(2)),imag(y(2)), real(y(3)),imag(y(3)));

yi = filter(1, [1 -0.5], [1+1i 2 4-1i]);
fprintf('IIR  y(2),y(3) = %g%+gi, %g%+gi  (expect 2.5+0.5i, 5.25-0.75i)\n', ...
        real(yi(2)),imag(yi(2)), real(yi(3)),imag(yi(3)));

yt = filter([1+1i 0.5], [1 0.2i], [1 2 3]);
fprintf('complex taps y(2) = %g%+gi  (expect 2.7+1.8i)\n', real(yt(2)), imag(yt(2)));

[yz, zf] = filter([1 1], 1, [1i 2i 3i]);
fprintf('zf = %g%+gi  (expect 0+3i)\n', real(zf(1)), imag(zf(1)));

yzi = filter([1 1], 1, [1i 2i 3i], 5+0i);
fprintf('zi  y(1) = %g%+gi  (expect 5+1i)\n', real(yzi(1)), imag(yzi(1)));

M  = [1i 2; 3i 4-1i];
ym = filter([1 1], 1, M);
fprintf('matrix y(2,:) = %g%+gi, %g%+gi  (expect 0+4i, 6-1i)\n', ...
        real(ym(2,1)),imag(ym(2,1)), real(ym(2,2)),imag(ym(2,2)));

r = filter([1 1], 1, [1 2 3]);
fprintf('real [1 3 5] real? %d\n', isreal(r));
