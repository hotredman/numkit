clear
import compat.*

fprintf('=== inv ===\n');
A = [4 7; 2 6];
B = inv(A);
disp(B);
fprintf('  A * inv(A) ~ I error: %g (expect ~ulp)\n\n', max(max(abs(A*B - eye(2)))));

fprintf('=== linsolve (square LU) ===\n');
A2 = [1 2 3; 4 5 6; 7 8 10];
b = [6; 15; 25];
x = linsolve(A2, b);
fprintf('  x = [%g %g %g]'' (expect [1 1 1]'')\n', x(1), x(2), x(3));
fprintf('  residual: %g\n\n', max(abs(A2 * x - b)));

fprintf('=== linsolve (tall least-squares) ===\n');
At = [1 2; 3 4; 5 6];
bt = [3; 7; 11];
xt = linsolve(At, bt);
fprintf('  x_LS = [%g %g]'' (expect [1 1]'')\n', xt(1), xt(2));
fprintf('  residual: %g\n\n', max(abs(At * xt - bt)));

fprintf('=== pageinv (3D) ===\n');
P = zeros(2, 2, 3);
P(:,:,1) = [4 7; 2 6];
P(:,:,2) = eye(2);
P(:,:,3) = [2 0; 0 4];
Q = pageinv(P);
fprintf('  Q(1,1,1)=%g (expect 0.6)\n', Q(1,1,1));
fprintf('  Q(2,2,2)=%g (expect 1)\n', Q(2,2,2));
fprintf('  Q(2,2,3)=%g (expect 0.25)\n', Q(2,2,3));
