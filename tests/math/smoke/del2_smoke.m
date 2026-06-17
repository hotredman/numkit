clear
import compat.*

% del2 — discrete Laplacian (MATLAB datafun). For each dimension a centered
% second difference (/2h^2) with linear edge extrapolation; summed over
% dimensions and divided by ndims (= 2 for both vectors and matrices).

% Vector: a quadratic ramp has a constant Laplacian.
v = del2([1 4 9 16 25]);
fprintf('del2 vector:    '); disp(v);          % expect [0.5 0.5 0.5 0.5 0.5]
fprintf('  sum = %.4f  (expect 2.5)\n', sum(v));

% Scalar spacing scales by 1/h^2.
v2 = del2([1 4 9 16 25], 2);
fprintf('del2(...,2)(1) = %.4f  (expect 0.125)\n', v2(1));

% 2-D matrix.
U = [1 2 3; 4 5 6; 7 8 10];
L = del2(U);
disp('del2 matrix:'); disp(L);
fprintf('  sum = %.4f  L(1,1) = %.4f  L(2,2) = %.4f  (expect 1.5, 0, 0)\n', ...
        sum(L(:)), L(1,1), L(2,2));

% magic(4): a near-linear surface -> Laplacian sums to zero.
M = magic(4); LM = del2(M);
fprintf('del2 magic(4) sum = %.4f  L(2,3) = %.4f  (expect 0, -3)\n', ...
        sum(LM(:)), LM(2,3));
