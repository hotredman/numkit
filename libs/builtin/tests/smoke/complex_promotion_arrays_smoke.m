clear
import compat.*

% sqrt / acosh / atanh on a real ARRAY with out-of-domain elements -> complex.
% Fixed 2026-06-05: previously only scalars promoted; array elements gave NaN.

s = sqrt([-1 4 -9]);
fprintf('sqrt([-1 4 -9]) cplx=%d: %g%+gi %g%+gi %g%+gi  (expect 0+1i 2+0i 0+3i)\n', ...
        ~isreal(s), real(s(1)),imag(s(1)), real(s(2)),imag(s(2)), real(s(3)),imag(s(3)));

h = acosh([0.5 2]);
fprintf('acosh([0.5 2]) cplx=%d: %g%+gi %g%+gi  (expect 0+1.0472i 1.31696+0i)\n', ...
        ~isreal(h), real(h(1)),imag(h(1)), real(h(2)),imag(h(2)));

t = atanh([2 -2 0.5]);
fprintf('atanh([2 -2 0.5]) cplx=%d: %g%+gi %g%+gi %g%+gi\n', ...
        ~isreal(t), real(t(1)),imag(t(1)), real(t(2)),imag(t(2)), real(t(3)),imag(t(3)));
fprintf('  (expect 0.5493+1.5708i, -0.5493-1.5708i, 0.5493+0i)\n');

% atanh(-2) scalar — imaginary sign must be -pi/2 (MATLAB), not +pi/2.
a = atanh(-2);
fprintf('atanh(-2) scalar = %g%+gi  (expect -0.5493-1.5708i)\n', real(a), imag(a));

% In-domain stays real.
fprintf('sqrt([1 4]) real? %d   acosh([1 2]) real? %d   atanh([0 0.5]) real? %d  (expect 1 1 1)\n', ...
        isreal(sqrt([1 4])), isreal(acosh([1 2])), isreal(atanh([0 0.5])));
