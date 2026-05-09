clear
import compat.*

fprintf('=== poly(matrix) -- characteristic polynomial ===\n');
A = [0 7 -6; 1 0 0; 0 1 0];   % companion of x^3 - 7x + 6
p = poly(A);
fprintf('  poly(A) = ['); fprintf(' %g', p); fprintf(' ]\n');
fprintf('  expect [1 0 -7 6]\n');

fprintf('\n=== eig(general) -- via char poly + roots ===\n');
e1 = eig(A);
fprintf('  eig(companion of x^3 - 7x + 6) = '); disp(e1');
fprintf('  expect roots {1, 2, -3}\n');

% Complex eigenvalues
B = [0 -1; 1 0];
e2 = eig(B);
fprintf('  eig([0 -1; 1 0]) = '); disp(e2');
fprintf('  expect ±i\n');

% 4x4 with mixed complex pairs
C = [1 2 0 0; -2 1 0 0; 0 0 3 1; 0 0 -1 3];
e3 = eig(C);
fprintf('  eig(block 4x4 with 2 complex pairs) = '); disp(e3');
