clear
import compat.*

fprintf('=== expmv = exp(t*A)*v via Krylov ===\n');

% Diagonal A: elementwise exact.
A = diag([1 2 3]);
w = expmv(0.5, A, [1; 1; 1]);
fprintf('diag([1 2 3]), t=0.5, v=ones:\n');
fprintf('  w = [%.6f %.6f %.6f]\n', w(1), w(2), w(3));
fprintf('  expected [exp(.5) exp(1) exp(1.5)] = [%.6f %.6f %.6f]\n', exp(0.5), exp(1.0), exp(1.5));

% Compare against full expm*v on triangular.
A2 = [0.1 0.2 0; 0 0.3 0.4; 0 0 0.5];
v2 = [1; 2; 3];
w_k = expmv(0.7, A2, v2);
w_f = expm(0.7 * A2) * v2;
fprintf('\ntriangular: ||w_krylov - w_full|| / ||w_full|| = %.3e\n', norm(w_k - w_f) / norm(w_f));

% t = 0 → identity.
v3 = [4; 7];
fprintf('\nt=0: w == v\n');
fprintf('  max|expmv(0, A, v) - v| = %.3e\n', max(abs(expmv(0.0, [2 1; -1 3], v3) - v3)));

% Linearity.
A3 = [1 0.5; -0.5 1];
v1 = [1; 0]; v2_l = [0; 1];
lhs = expmv(0.3, A3, v1 + v2_l);
rhs = expmv(0.3, A3, v1) + expmv(0.3, A3, v2_l);
fprintf('\nlinearity: ||expmv(t, A, v1+v2) - (expmv(t,A,v1)+expmv(t,A,v2))|| = %.3e\n', norm(lhs - rhs));

fprintf('\n(NOTE: MATLAB core ships no `expmv` — only Higham File Exchange package does;\n');
fprintf('       parity spec reports correctness=N/A; algebraic-identity tests above stand.)\n');
