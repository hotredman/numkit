clear
import compat.*
% filter — final state output [y,zf] + initial conditions filter(b,a,x,zi).
[y, zf] = filter([1 1], [1 -0.5], [1 2 3 4]);
fprintf('y: %g %g %g %g  zf=%g (expect 1 3.5 6.75 10.375 / 9.1875)\n', y(1),y(2),y(3),y(4), zf(1));

yi = filter([1 1], [1 -0.5], [1 2 3 4], 10);
fprintf('zi=10 -> y(1)=%g (expect 11)\n', yi(1));

[y2, zf2] = filter([1 0.5 0.25], 1, [1 2 3 4 5]);
fprintf('FIR zf2 numel=%d: %g %g (expect 2 / 3.5 1.25)\n', numel(zf2), zf2(1), zf2(2));
