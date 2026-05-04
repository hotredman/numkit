import compat.*

% --- Continuous Lyapunov on a stable scalar: A·X + X·A + Q = 0 ---
% A = -2, Q = 4 → X = -Q/(2A) = -4/-4 = 1.
A = -2;
Q = 4;
X = lyap(A, Q);
fprintf('--- lyap(-2, 4) ---\n');
fprintf('  X = %.6f (expect 1.0)\n\n', X);

% Verify residual: A·X + X·Aᵀ + Q ≈ 0
res = A*X + X*A' + Q;
fprintf('  residual = %.2e (expect ~ 0)\n\n', res);

% --- 2x2 stable case ---
A = [-2 1; 0 -3];
Q = eye(2);
X = lyap(A, Q);
fprintf('--- lyap(2x2 stable, I) ---\n');
disp(X);
res = A*X + X*A' + Q;
fprintf('  max(|residual|) = %.2e\n\n', max(max(abs(res))));

% --- Symmetry: X should be symmetric for symmetric Q ---
fprintf('  symmetry err = %.2e\n\n', max(max(abs(X - X'))));

% --- Continuous Lyapunov w/ pos-def Q on stable A is pos-def ---
A = [-1 2; -3 -4];
Q = eye(2);
X = lyap(A, Q);
% Pos-def via leading principal minors (Sylvester criterion):
m1 = X(1,1);
m2 = X(1,1)*X(2,2) - X(1,2)*X(2,1);
fprintf('--- pos-def check on lyap(stable, I) ---\n');
fprintf('  X(1,1) = %.4f (>0?), det(X) = %.4f (>0?)\n', m1, m2);
fprintf('  expect both > 0 (Sylvester criterion)\n\n');

% --- Discrete Lyapunov: A·X·Aᵀ - X + Q = 0 ---
% A = 0.5, Q = 1 → X = Q/(1 - A²) = 1/(1-0.25) = 4/3.
A = 0.5;
Q = 1;
X = dlyap(A, Q);
fprintf('--- dlyap(0.5, 1) ---\n');
fprintf('  X = %.6f (expect 4/3 = 1.3333)\n\n', X);

% Verify residual.
res = A*X*A' - X + Q;
fprintf('  residual = %.2e (expect ~ 0)\n\n', res);

% --- 2x2 stable discrete ---
A = [0.5 0.1; 0 0.7];
Q = eye(2);
X = dlyap(A, Q);
fprintf('--- dlyap(2x2 stable, I) ---\n');
disp(X);
res = A*X*A' - X + Q;
fprintf('  max(|residual|) = %.2e\n', max(max(abs(res))));

% --- Detect singular case (unstable A): expect error or huge values ---
try
    Au = [1 0; 0 2];   % unstable continuous
    Xu = lyap(Au, eye(2));
    fprintf('\n--- lyap on unstable A ---\n');
    fprintf('  X = '); disp(Xu);
    fprintf('  (linear system might still solve, but X has no physical meaning)\n');
catch err
    fprintf('\n--- lyap on unstable correctly errored: %s\n', err.message);
end
