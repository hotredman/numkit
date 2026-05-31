clear

import compat.*

% pchip 2-arg pp-form (2026-05-30): MATLAB pchip(x,y) (no query point)
% returns a piecewise-polynomial struct usable with ppval, exactly like
% spline(x,y). numkit's spline(x,y) already did this, but pchip(x,y)
% errored "requires 3 arguments". Now pchip(x,y) builds the same pp
% struct from the shape-preserving Hermite derivatives. vs MATLAB R2025b.

x = [1 2 3 4];  y = [1 4 9 16];
pp = pchip(x, y);

fprintf('=== pp struct fields ===\n');
fprintf('class=%s order=%d pieces=%d\n', class(pp), pp.order, pp.pieces);
fprintf('coefs (rows=pieces, cols a b c d for dx^3..dx^0):\n');
disp(pp.coefs);
fprintf('expect row1 = [-0.25 1.25 2 1]\n\n');

fprintf('=== ppval(pp, .) matches the value form pchip(x,y,xq) ===\n');
fprintf('ppval 2.5 = %.10f  (expect 6.2395833333)\n', ppval(pp, 2.5));
fprintf('ppval 1.5 = %.10f  (expect 2.28125)\n',      ppval(pp, 1.5));
fprintf('ppval 3.2 = %.10f  (expect 10.2186666667)\n', ppval(pp, 3.2));
fprintf('pchip(x,y,2.7) = %.10f  vs  ppval(pp,2.7) = %.10f\n', ...
        pchip(x, y, 2.7), ppval(pp, 2.7));

fprintf('\n=== non-uniform grid ===\n');
pp2 = pchip([0 1 3 4], [2 1 4 3]);
fprintf('ppval 2.0 = %.10f  (expect 2.5)\n',          ppval(pp2, 2.0));
fprintf('ppval 0.5 = %.10f  (expect 1.2708333333)\n', ppval(pp2, 0.5));

fprintf('\n=== 2 points -> straight line ===\n');
fprintf('ppval 1.5 = %.10f  (expect 4)\n', ppval(pchip([1 2], [3 5]), 1.5));
