clear
import compat.*

% log1p(x) = log(1+x) goes complex for x < -1. Fixed 2026-06-05
% (bugs/builtin/log-complex-promotion-arrays.md, log1p follow-up).
% Reference: MATLAB R2025b.

s = log1p(-2);
fprintf('log1p(-2)        cplx=%d: %g%+gi  (expect 0+3.14159i = log(-1))\n', ...
        ~isreal(s), real(s), imag(s));

a = log1p([-2 -0.5 0 3]);
fprintf('log1p([-2 -0.5 0 3]): %g%+gi %g%+gi %g%+gi %g%+gi\n', ...
        real(a(1)),imag(a(1)), real(a(2)),imag(a(2)), real(a(3)),imag(a(3)), real(a(4)),imag(a(4)));
fprintf('   (expect 0+3.14159i, -0.69315+0i, 0+0i, 1.38629+0i)\n');

b = log1p([-2, 1e-15]);
fprintf('accuracy in promoted array: real(b(2)) = %.3e  (expect 1e-15, NOT 1.11e-15)\n', real(b(2)));

z = log1p(3+4i);
fprintf('log1p(3+4i)      = %.6f%+.6fi  (expect 1.732868+0.785398i)\n', real(z), imag(z));

fprintf('in-domain log1p([0 1 5]) real? %d   log1p(-1)=%g (expect 1, -Inf)\n', ...
        isreal(log1p([0 1 5])), log1p(-1));
