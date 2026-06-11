clear
import compat.*

fprintf('=== balance ===\n');

% Classic Parlett-Reinsch test
A = [1 100 10000; 0.01 1 100; 0.0001 0.01 1];
fprintf('input A:\n'); disp(A);

[T, B] = balance(A);
fprintf('T (diag scaling):\n'); disp(T);
fprintf('B (balanced):\n'); disp(B);
fprintf('  expect T = diag([512, 8, 0.0625])\n');
fprintf('  residual ||B - inv(T)*A*T|| = %.2e\n', norm(B - T\A*T));

% Verify eigvals preserved
fprintf('  eig(A): '); fprintf('%.4f ', sort(real(eig(A)))); fprintf('\n');
fprintf('  eig(B): '); fprintf('%.4f ', sort(real(eig(B)))); fprintf('  (must match)\n');

% 1-out form
fprintf('\n1-out: B = balance(A)\n');
B1 = balance(A);
disp(B1)

% 3-out form [S, P, B]
fprintf('\n3-out: [S, P, B] = balance(A)\n');
[S, P, B3] = balance(A);
fprintf('  S = '); fprintf('%g ', S); fprintf(' (expect [512 8 0.0625])\n');
fprintf('  P = '); fprintf('%d ', P); fprintf(' (expect [1 2 3])\n');

% noperm option
fprintf('\nnoperm option:\n');
[T2, B2] = balance(A, 'noperm');
fprintf('T2 should equal T:\n'); disp(T2);

% Already balanced (identity)
fprintf('\neye(3):\n');
[Te, Be] = balance(eye(3));
fprintf('T eye:\n'); disp(Te);

% Simple 2x2 with extreme scaling
fprintf('\n2x2 large dynamic range:\n');
A2x2 = [1 1e6; 1e-6 1];
[T22, B22] = balance(A2x2);
fprintf('T:\n'); disp(T22);
fprintf('B:\n'); disp(B22);
