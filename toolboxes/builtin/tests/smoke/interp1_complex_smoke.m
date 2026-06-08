clear
import compat.*

% interp1 of a COMPLEX y — interpolate real + imaginary parts separately.
% Fixed 2026-06-05: previously "Not a double array".

y = interp1([1 2 3], [1+1i 2+2i 3+3i], 2.5);
fprintf('linear  = %g%+gi   (expect 2.5+2.5i)\n', real(y), imag(y));

yv = interp1([1 2 3], [1+1i 4 3-2i], [1.5 2.5]);
fprintf('vector  = %g%+gi, %g%+gi   (expect 2.5+0.5i, 3.5-1i)\n', ...
        real(yv(1)),imag(yv(1)), real(yv(2)),imag(yv(2)));

yn = interp1([1 2 3], [1+1i 5 3-2i], 2.4, 'nearest');
fprintf('nearest = %g%+gi   (expect 5+0i)\n', real(yn), imag(yn));

ys = interp1([1 2 3 4], [1+1i 0 -1+2i 3], 2.5, 'spline');
fprintf('spline  = %g%+gi   (expect -0.8125+1.0625i)\n', real(ys), imag(ys));

ye = interp1([1 2 3], [1+1i 2+2i 3+3i], 5);
fprintf('extrap  = %g%+gi   (expect NaN+NaNi)\n', real(ye), imag(ye));

M = [1+1i 2; 3 4-1i; 5+5i 6];
ym = interp1([1 2 3], M, 1.5);
fprintf('matrix  = %g%+gi, %g%+gi   (expect 2+0.5i, 3-0.5i)\n', ...
        real(ym(1)),imag(ym(1)), real(ym(2)),imag(ym(2)));
