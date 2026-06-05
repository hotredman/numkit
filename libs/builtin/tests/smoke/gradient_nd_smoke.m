clear
import compat.*

% gradient of N-D (3-D / 4-D) arrays. Fixed 2026-06-05
% (bugs/builtin/gradient-3d.md). Reference: MATLAB R2025b.

A = reshape(1:8, 2, 2, 2);

g = gradient(A);
fprintf('single gradient(A) g(1,1,1) = %g   (expect 2, the x/dim-2 gradient)\n', g(1,1,1));

[gx, gy, gz] = gradient(A);
fprintf('[gx,gy,gz](1,1,1)  = %g %g %g   (expect 2 1 4)\n', gx(1,1,1), gy(1,1,1), gz(1,1,1));

B = reshape(1:27, 3, 3, 3);
[bx, by, bz] = gradient(B);
fprintf('central (3x3x3)    : bx(1,2,1)=%g by(2,1,1)=%g bz(1,1,2)=%g   (expect 3 1 9)\n', ...
        bx(1,2,1), by(2,1,1), bz(1,1,2));

[hx, hy, hz] = gradient(A, 2, 3, 4);
fprintf('per-dim spacing    : hx=%g hy=%.4f hz=%g   (expect 1 0.3333 1)\n', ...
        hx(1,1,1), hy(1,1,1), hz(1,1,1));

[sa, sb, sc] = gradient(A, 2);
fprintf('single-spacing bcast: %g %g %g   (expect 1 0.5 2)\n', sa(1,1,1), sb(1,1,1), sc(1,1,1));

C = reshape(1:12, 2, 3, 2);
[cx, cy, cz] = gradient(C);
fprintf('non-cube 2x3x2     : cx(1,2,1)=%g cy(2,1,1)=%g cz(1,1,2)=%g   (expect 2 1 6)\n', ...
        cx(1,2,1), cy(2,1,1), cz(1,1,2));

D = reshape(1:16, 2, 2, 2, 2);
[d1, d2, d3, d4] = gradient(D);
fprintf('4-D                : d1(1,1,1,1)=%g d4(1,1,1,1)=%g   (expect 2 8)\n', ...
        d1(1,1,1,1), d4(1,1,1,1));

Z = reshape(1:8, 2, 2, 2) + 1i*reshape(8:-1:1, 2, 2, 2);
[zx, zy, zz] = gradient(Z);
fprintf('complex 3-D        : zx(1,1,1) = %g%+gi   (expect 2-2i)\n', real(zx(1,1,1)), imag(zx(1,1,1)));
