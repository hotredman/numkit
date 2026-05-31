clear

import compat.*

% makima 2-arg pp-form (2026-05-30): MATLAB makima(x,y) (no query point)
% returns a piecewise-polynomial struct usable with ppval, exactly like
% spline(x,y) and pchip(x,y). numkit's spline/pchip 2-arg already did
% this, but makima(x,y) errored "pp-form (2-arg) not yet supported". Now
% makima(x,y) builds the same pp struct from the modified-Akima Hermite
% derivatives. vs MATLAB R2025b.

x = [1 2 3 4 5];  y = [1 4 9 16 25];
pp = makima(x, y);

fprintf('=== pp struct fields ===\n');
fprintf('class=%s order=%d pieces=%d\n', class(pp), pp.order, pp.pieces);
fprintf('coefs (rows=pieces, cols a b c d for dx^3..dx^0):\n');
disp(pp.coefs);
fprintf('expect row1 ~= [-0.83333 2.33333 1.5 1]\n\n');

fprintf('=== ppval(pp, .) matches the value form makima(x,y,xq) ===\n');
fprintf('ppval 2.5 = %.10f  (expect 6.2395833333)\n',  ppval(pp, 2.5));
fprintf('ppval 3.7 = %.10f  (expect 13.70365)\n',       ppval(pp, 3.7));
fprintf('makima(x,y,3.3) = %.10f  vs  ppval(pp,3.3) = %.10f\n', ...
        makima(x, y, 3.3), ppval(pp, 3.3));

fprintf('\n=== non-uniform grid ===\n');
pp2 = makima([0 1 3 4 7], [2 1 4 3 8]);
fprintf('ppval 2.0 = %.10f  (expect 2.5697463768)\n', ppval(pp2, 2.0));
fprintf('ppval 0.5 = %.10f  (expect 1.2161458333)\n', ppval(pp2, 0.5));

fprintf('\n=== 3 points ===\n');
p3 = makima([1 2 3], [2 5 4]);
fprintf('ppval 1.5 = %.10f  (expect 3.9201388889)\n', ppval(p3, 1.5));
fprintf('ppval 2.5 = %.10f  (expect 4.875)\n',         ppval(p3, 2.5));
