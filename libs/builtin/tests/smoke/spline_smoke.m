clear
import compat.*
% spline n==3: MATLAB's not-a-knot end conditions reduce to the unique
% interpolating PARABOLA, so a 3-point spline of x^2 reproduces x^2 exactly.
% (numkit used to fall back to natural BCs here -> 6.3125, wrong.)
fprintf('spline([1 2 3],[1 4 9],2.5) = %.4f  (expect 6.2500 = 2.5^2)\n', ...
        spline([1 2 3], [1 4 9], 2.5));
fprintf('spline([1 2 3],[1 4 9],1.5) = %.4f  (expect 2.2500)\n', ...
        spline([1 2 3], [1 4 9], 1.5));

% Non-uniform knots: parabola x^2 + 1 through (0,1),(1,2),(3,10).
fprintf('spline([0 1 3],[1 2 10],2)  = %.4f  (expect 5.0000)\n', ...
        spline([0 1 3], [1 2 10], 2));

% Downward parabola, symmetric about x=2.
fprintf('spline([1 2 3],[0 1 0],1.5) = %.4f  (expect 0.7500)\n', ...
        spline([1 2 3], [0 1 0], 1.5));

% n >= 4 was already correct (unchanged).
fprintf('spline n=5 (2.5)            = %.4f  (expect 6.2500)\n', ...
        spline([1 2 3 4 5], [1 4 9 16 25], 2.5));
