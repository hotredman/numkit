clear
import compat.*

% acos / asin of a REAL argument outside [-1,1] -> complex (fixed 2026-06-05).
% Previously returned NaN. Computed via acosh to match MATLAB's branch cut.

a = acos(2);
fprintf('acos(2)  = %.5g%+.5gi   (expect 0+1.31696i)\n', real(a), imag(a));
b = asin(2);
fprintf('asin(2)  = %.5g%+.5gi   (expect 1.5708-1.31696i)\n', real(b), imag(b));
c = acos(-2);
fprintf('acos(-2) = %.5g%+.5gi   (expect 3.14159-1.31696i)\n', real(c), imag(c));
d = asin(-2);
fprintf('asin(-2) = %.5g%+.5gi   (expect -1.5708+1.31696i)\n', real(d), imag(d));

% Any out-of-range element promotes the whole array.
v = acos([0.5 2]);
fprintf('acos([0.5 2]) iscomplex=%d  el1=%.5g%+.5gi  el2=%.5g%+.5gi\n', ...
        ~isreal(v), real(v(1)),imag(v(1)), real(v(2)),imag(v(2)));
fprintf('  (expect complex; 1.0472+0i, 0+1.31696i)\n');

% In-domain input stays real.
fprintf('acos(0.5) real? %d  value=%.5g  (expect 1, 1.0472)\n', isreal(acos(0.5)), acos(0.5));
