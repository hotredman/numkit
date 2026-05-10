clear
import compat.*

fprintf('=== lsqminnorm ===\n');

% Full-rank square
A1 = [1 2; 3 4]; B1 = [5; 11];
x1 = lsqminnorm(A1, B1);
fprintf('  full-rank 2x2 x = '); fprintf('%g ', x1); fprintf(' (expect [1 2])\n');

% Rank-deficient (rank 1)
A2 = [1 2; 2 4; 3 6]; B2 = [3; 6; 9];
x2 = lsqminnorm(A2, B2);
fprintf('  rank-1 x = '); fprintf('%g ', x2); fprintf('\n');
fprintf('  expect min-norm [0.6; 1.2], ||A*x - b|| ~ 0\n');
fprintf('  ||A*x - b|| = %.2e, ||x|| = %g\n', norm(A2*x2 - B2), norm(x2));

% Wide system
A3 = [1 2 3; 4 5 6]; B3 = [6; 15];
x3 = lsqminnorm(A3, B3);
fprintf('  wide 2x3 x = '); fprintf('%g ', x3); fprintf('\n');
fprintf('  ||A*x - b|| = %.2e, ||x|| = %g\n', norm(A3*x3 - B3), norm(x3));

% B = matrix
A4 = [1 0; 0 0]; B4 = [1 2; 3 4];
X4 = lsqminnorm(A4, B4);
fprintf('  rank-1 with B matrix:\n'); disp(X4);

fprintf('\n=== lsqnonneg ===\n');

% Classic Lawson-Hanson example
C1 = [1 -1 2; 3 4 5; 6 7 8];
d1 = [1; 2; 3];
[x1, rn1, res1, ef1, out1] = lsqnonneg(C1, d1);
fprintf('  C=[1 -1 2; 3 4 5; 6 7 8], d=[1 2 3]\n');
fprintf('  x = '); fprintf('%g ', x1); fprintf('  (expect [0 0 0.387097])\n');
fprintf('  resnorm = %.10g (expect 0.0645161)\n', rn1);
fprintf('  residual = '); fprintf('%g ', res1); fprintf('\n');
fprintf('  exitflag = %d (expect 1)\n', ef1);
fprintf('  out.iterations = %d, out.algorithm = %s\n', out1.iterations, out1.algorithm);

% 2x2 with negative target
[x2] = lsqnonneg(eye(2), [3; -2]);
fprintf('\n  2x2 negative target x = '); fprintf('%g ', x2); fprintf(' (expect [3 0])\n');

% Already nonneg
[x3] = lsqnonneg([1 0; 0 1; 1 1], [1; 2; 3]);
fprintf('  already-nonneg x = '); fprintf('%g ', x3); fprintf(' (expect [1 2])\n');

% Zero RHS
[x4, rn4] = lsqnonneg([1 2; 3 4], [0; 0]);
fprintf('  zero RHS x = '); fprintf('%g ', x4); fprintf('  resnorm = %g (expect 0)\n', rn4);
