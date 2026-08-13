% solving_linear_systems.m
% 
% Demonstrates solving systems of linear equations of the form A*x = b.
% Shows the difference between using the matrix left division operator (\) 
% and using the matrix inverse, highlighting the performance and numerical 
% stability of the backslash operator.

disp('--- Solving Linear Systems A*x = b ---');

% 1. Create a well-conditioned random square matrix A and a vector b
N = 1000;
A = rand(N, N) + N * eye(N); % Make it diagonally dominant to ensure it's well-conditioned
x_true = rand(N, 1);
b = A * x_true;

% 2. Solve using the backslash operator (\)
% This is the recommended way to solve A*x = b in numkit.
% It uses LU decomposition (via BLAS/LAPACK) under the hood.
disp('Solving using backslash (\):');
tic;
x_backslash = A \ b;
time_backslash = toc;
fprintf('Time taken: %.4f seconds\n', time_backslash);

% Measure the error and residual
err_backslash = norm(x_backslash - x_true);
res_backslash = norm(A * x_backslash - b);
fprintf('Solution error ||x - x_true||: %e\n', err_backslash);
fprintf('Residual ||A*x - b||:         %e\n\n', res_backslash);

% 3. Solve using the matrix inverse (inv)
% Generally not recommended for solving linear systems due to higher 
% computational cost and lower numerical precision compared to \
disp('Solving using matrix inverse (inv):');
tic;
x_inv = inv(A) * b;
time_inv = toc;
fprintf('Time taken: %.4f seconds\n', time_inv);

% Measure the error and residual
err_inv = norm(x_inv - x_true);
res_inv = norm(A * x_inv - b);
fprintf('Solution error ||x - x_true||: %e\n', err_inv);
fprintf('Residual ||A*x - b||:         %e\n\n', res_inv);

% 4. Comparison
fprintf('Backslash is %.2fx faster than inverse for a %dx%d matrix.\n', time_inv / time_backslash, N, N);

% 5. Overdetermined systems (Least Squares)
% The \ operator can also solve overdetermined systems (more equations than unknowns)
% using QR decomposition to find the least squares solution.
disp('--- Solving Overdetermined Systems (Least Squares) ---');
M = 500;
N_vars = 100;
A_rect = rand(M, N_vars);
x_exact = rand(N_vars, 1);
b_rect = A_rect * x_exact + 0.1 * randn(M, 1); % Add some noise

x_ls = A_rect \ b_rect;
fprintf('Least squares error ||x_ls - x_exact||: %e\n', norm(x_ls - x_exact));
