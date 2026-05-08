clear

import compat.*

% Misc batch 5 — poly + string-extras2 + math2 + error-handling. ТЗ closure 2026-05-09.

p = polyfit([1 2 3 4], [1 4 9 16], 2);
fprintf('polyfit y=x^2: p = '); disp(p);
fprintf('polyval([1 0 0], 5) = %g  (expect 25)\n', polyval([1 0 0], 5));
pp = mkpp([0 1 2], [1 0; 1 0]);
fprintf('ppval(pp, 0.5) = %g\n', ppval(pp, 0.5));

fprintf('join(["a","b","c"]) = "%s"\n', join(["a","b","c"]));
fprintf('replace("hello","l","X") = "%s"\n', replace("hello","l","X"));
fprintf('matches("hello","hello") = %d\n', matches("hello","hello"));

s.a=1; s.b=2; t = rmfield(s, "a");
fprintf('rmfield s.a → fieldnames: '); disp(fieldnames(t));

P = legendre(2, 0.5);
fprintf('legendre(2, 0.5) = '); disp(P);
fprintf('psi(1) = %.15f  (expect -0.577216)\n', psi(1));
fprintf('realmax = %g, realmin = %g\n', realmax, realmin);
assert(true);
fprintf('assert(true) — no-op OK\n');
