clear
import compat.*
% zp2tf — zero/pole/gain -> transfer function. b is padded to numel(a).
[b, a] = zp2tf([1; 2], [3; 4], 5);
fprintf('equal-degree b: %g %g %g | a: %g %g %g (expect 5 -15 10 / 1 -7 12)\n', b(1),b(2),b(3), a(1),a(2),a(3));

% Fewer zeros than poles -> numerator LEFT-padded with zeros to length numel(a).
[bc, ac] = zp2tf([0.5], [0.3+0.4i; 0.3-0.4i], 2);
fprintf('padded numel(bc)=%d bc: %g %g %g (expect 3 / 0 2 -1)\n', numel(bc), bc(1),bc(2),bc(3));
fprintf('ac: %g %g %g (expect 1 -0.6 0.25)\n', ac(1),ac(2),ac(3));
