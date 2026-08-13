% matrix_properties.m
% 
% Demonstrates functions for calculating various matrix properties.
% These include norms, condition numbers, determinants, rank, and trace.

A = magic(4);
disp('Matrix A (Magic square of size 4):');
disp(A);

% 1. Norm
% norm() calculates the 2-norm (maximum singular value) of a matrix by default
n2 = norm(A);
fprintf('2-Norm of A:               %f\n', n2);

% For vectors, norm calculates the Euclidean norm
v = [3, 4];
fprintf('Euclidean norm of [3, 4]:  %f\n\n', norm(v));

% 2. Condition Number
% cond() calculates the condition number for inversion.
% A large condition number indicates that the matrix is ill-conditioned 
% (nearly singular), meaning that solving A*x = b will be highly sensitive 
% to numerical errors or noise in b.
c = cond(A);
fprintf('Condition number of A:     %f\n', c);

ill_cond_matrix = [1, 1; 1, 1.000000001];
fprintf('Condition number of nearly singular matrix:\n                           %e\n\n', cond(ill_cond_matrix));


% 3. Determinant
% det() calculates the determinant of a square matrix.
d = det(A);
fprintf('Determinant of A:          %f\n', d);


% 4. Matrix Rank
% rank() provides an estimate of the number of linearly independent rows or columns.
% It is computed using Singular Value Decomposition (SVD).
r = rank(A);
fprintf('Rank of A:                 %d\n', r);

rank_deficient_matrix = [1, 2, 3; 2, 4, 6; 1, 1, 1];
fprintf('Rank of rank-deficient matrix (col2 = 2*col1):\n                           %d\n\n', rank(rank_deficient_matrix));


% 5. Matrix Trace
% trace() is the sum of the diagonal elements of a square matrix.
% For a magic square of size N, the trace equals the magic constant.
t = trace(A);
fprintf('Trace of A:                %f\n', t);
