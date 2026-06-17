clear
import compat.*

fprintf('=== sylvester (symmetric A and B) ===\n');
A = [4 1; 1 3];
B = [2 0.5; 0.5 5];
C = [1 2; 3 4];
X = sylvester(A, B, C);
fprintf('  X =\n'); disp(X);
fprintf('  residual A*X + X*B - C: %g (expect ~0)\n', max(max(abs(A*X + X*B - C))));

fprintf('\n=== diagonal closed-form ===\n');
A2 = [3 0; 0 5];
B2 = [1 0; 0 4];
C2 = [10 6; 8 18];
X2 = sylvester(A2, B2, C2);
fprintf('  X2 = [%g %g; %g %g]\n', X2(1,1), X2(1,2), X2(2,1), X2(2,2));
fprintf('  expect closed-form X2(i,j) = C2(i,j)/(d_a_i + d_b_j):\n');
fprintf('    [10/4 6/7; 8/6 18/9] = [2.5 0.857143 1.33333 2]\n');
