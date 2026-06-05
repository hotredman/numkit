clear

import compat.*

% bugs/builtin/psi-zero-pole.md — psi(0) is the digamma pole at 0: MATLAB
% returns -Inf (was NaN). Finite positive values + the negative-domain
% behaviour are unchanged.

fprintf('psi(0)  = %.8g   expect -Inf  (isinf=%d, <0 -> %d)\n', psi(0), isinf(psi(0)), psi(0) < 0);
fprintf('psi(1)  = %.8g   expect -0.57721566\n', psi(1));
fprintf('psi(2)  = %.8g   expect 0.42278434\n', psi(2));
fprintf('psi(0.5)= %.6g   expect -1.96351\n', psi(0.5));
fprintf('psi(10) = %.8g   expect 2.2517526\n', psi(10));
v = psi([0 1 2]);
fprintf('psi([0 1 2]) = [%.6g %.6g %.6g]   expect [-Inf -0.577216 0.422784]\n', v(1), v(2), v(3));
