clear
import compat.*

% interpn 1-D forms — bugs/math/interpn-nan: 1-D used to return NaN; now the
% 1-D path delegates to interp1. 2-D/3-D dispatch unchanged.

% Form B: interpn(X, V, Xq) — grid + values + query
y1 = interpn([1 2 3], [1 4 9], 2.5);
fprintf('interpn([1 2 3],[1 4 9],2.5) = %g   (expect 6.5)\n', y1);

% NB interpn(V, scalar) is the grid-REFINEMENT form in MATLAB (not a query),
% which numkit does not implement — so it is intentionally not shown here.

% Vector query + explicit method (Form B)
y3 = interpn([1 2 3 4], [10 20 30 40], [1.5 3.5], 'linear');
fprintf('interpn(...,[1.5 3.5])       = [%g %g]   (expect [15 35])\n', y3(1), y3(2));

% 3-D ndgrid Form B (unchanged path) — linear field X+Y+Z is exact
[X, Y, Z] = ndgrid(1:5, 1:5, 1:5); V = X + Y + Z;
y4 = interpn(X, Y, Z, V, 2.5, 2.5, 2.5);
fprintf('interpn 3-D (X+Y+Z) @2.5^3   = %g   (expect 7.5)\n', y4);
