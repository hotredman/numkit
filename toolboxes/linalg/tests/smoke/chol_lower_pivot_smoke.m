clear
import compat.*
% chol 'lower'/'upper' option + [R,p] second output (MATLAB R2025b).

A = [4 2 2; 2 5 1; 2 1 6];
R = chol(A);            % default 'upper': R'*R = A
L = chol(A, 'lower');   % L*L' = A, lower triangular (= R')
fprintf('R diag = [%g %g %g] (expect 2 2 2.23607)\n', R(1,1), R(2,2), R(3,3));
fprintf('L      = [%g %g %g; %g %g %g; %g %g %g]\n', ...
        L(1,1),L(1,2),L(1,3), L(2,1),L(2,2),L(2,3), L(3,1),L(3,2),L(3,3));
fprintf('  expect [2 0 0; 1 2 0; 1 0 2.23607]\n');
fprintf('||L*L'' - A|| = %g (expect ~0)\n', norm(L*L' - A, 'fro'));

% Second output p: 0 if positive-definite.
[Rp, p] = chol(A);
fprintf('[R,p] on PD: p = %g (expect 0)\n', p);

% Not positive-definite: [R,p] returns p = failure column, no error.
[R2, p2] = chol([1 3; 3 1]);
fprintf('[R,p] on indefinite: p = %g, size(R) = [%g %g] (expect 2, [1 1])\n', ...
        p2, size(R2,1), size(R2,2));
