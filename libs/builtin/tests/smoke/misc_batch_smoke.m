clear

import compat.*

% Misc batch — predicates + airy + coord conversion. Audit ТЗ closure 2026-05-09.

fprintf('allfinite([1 NaN 3]) = %d  (expect 0)\n',  allfinite([1 NaN 3]));
fprintf('allunique([1 2 1])   = %d  (expect 0)\n',  allunique([1 2 1]));
fprintf('anynan([1 NaN 3])    = %d  (expect 1)\n',  anynan([1 NaN 3]));
fprintf('airy(0)              = %.15f  (expect 0.355028)\n', airy(0));
[th, rh] = cart2pol(1, 1);
fprintf('cart2pol(1,1)        = th=%.15f, rh=%.15f  (expect pi/4, sqrt(2))\n', th, rh);
[az, el, rh] = cart2sph(1, 1, 0);
fprintf('cart2sph(1,1,0)      = az=%.15f, el=%.15f, rh=%.15f\n', az, el, rh);
