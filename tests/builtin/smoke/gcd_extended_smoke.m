clear

import compat.*

% [g,u,v] = gcd(a,b): extended GCD / Bezout coefficients with a*u + b*v = g.
% 3-output form added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== extended GCD (Bezout) ===\n');
[g,u,v] = gcd(12, 18);
fprintf('[g,u,v]=gcd(12,18)  -> g=%g u=%g v=%g  check 12*u+18*v=%g (expect 6 -1 1, =6)\n', g,u,v, 12*u+18*v);
[g,u,v] = gcd(8, 5);
fprintf('[g,u,v]=gcd(8,5)    -> g=%g u=%g v=%g  check=%g (expect 1 2 -3, =1)\n', g,u,v, 8*u+5*v);

fprintf('\n=== negatives keep g>=0, normalize coeffs ===\n');
[g,u,v] = gcd(-12, 18);
fprintf('[g,u,v]=gcd(-12,18) -> g=%g u=%g v=%g (expect 6 1 1)\n', g,u,v);
[g,u,v] = gcd(12, -18);
fprintf('[g,u,v]=gcd(12,-18) -> g=%g u=%g v=%g (expect 6 -1 -1)\n', g,u,v);

fprintf('\n=== zeros ===\n');
[g,u,v] = gcd(0, 5);
fprintf('[g,u,v]=gcd(0,5)    -> g=%g u=%g v=%g (expect 5 0 1)\n', g,u,v);
[g,u,v] = gcd(0, 0);
fprintf('[g,u,v]=gcd(0,0)    -> g=%g u=%g v=%g (expect 0 0 0)\n', g,u,v);

fprintf('\n=== elementwise over vectors ===\n');
[g,u,v] = gcd([12 8], [18 5]);
fprintf('g=%s u=%s v=%s (expect [6 1] [-1 2] [1 -3])\n', mat2str(g), mat2str(u), mat2str(v));

fprintf('\n=== 1-output regress ===\n');
fprintf('gcd(12,18)=%g  lcm(4,6)=%g\n', gcd(12,18), lcm(4,6));
