clear
import compat.*
% trapz — matrix per-column, trapz(Y,dim), trapz(X,Y,dim) (MATLAB R2025b).
fprintf('vector       : %g (expect 21.5)\n', trapz([1 4 9 16]));
fprintf('x-spacing    : %g (expect 21.5)\n', trapz([0 1 2 3], [1 4 9 16]));

tc = trapz([1 2 3; 4 5 6]);          % per column (dim 1)
fprintf('trapz(M) cols: %g %g %g (expect 2.5 3.5 4.5)\n', tc(1), tc(2), tc(3));

tr = trapz([1 2 3; 4 5 6], 2);       % per row (dim 2)
fprintf('trapz(M,2)   : %g %g (expect 4 10)\n', tr(1), tr(2));

tx = trapz([10 20 30], [1 2 3; 4 5 6], 2);
fprintf('trapz(X,M,2) : %g %g (expect 40 100)\n', tx(1), tx(2));
