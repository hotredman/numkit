clear

import compat.*

% sign() of a complex value: z/|z| for z != 0, else 0 (MATLAB R2025b).
% Complex support added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== complex sign = z/|z| ===\n');
a = sign(3-4i);
fprintf('sign(3-4i)  = %g%+gi (expect 0.6-0.8i)\n', real(a), imag(a));
b = sign(1i);
fprintf('sign(1i)    = %g%+gi (expect 0+1i)\n', real(b), imag(b));
c = sign(complex(0,0));
fprintf('sign(0+0i)  = %g%+gi (expect 0+0i, zero stays zero)\n', real(c), imag(c));
e = sign(2+2i);
fprintf('sign(2+2i)  = %.6f%+.6fi (expect 0.707107+0.707107i)\n', real(e), imag(e));

fprintf('\n=== complex vector ===\n');
d = sign([3+4i, 0, -2i]);
fprintf('sign([3+4i 0 -2i]):\n');
for k = 1:numel(d)
    fprintf('  (%d) = %g%+gi\n', k, real(d(k)), imag(d(k)));
end
fprintf('  expect [0.6+0.8i, 0+0i, 0-1i]\n');

fprintf('\n=== real / integer regress ===\n');
fprintf('sign(-3.5)=%g sign(2.1)=%g sign(0)=%g\n', sign(-3.5), sign(2.1), sign(0));
fprintf('sign(int8(-5))=%g class=%s (expect -1 int8)\n', double(sign(int8(-5))), class(sign(int8(-5))));
