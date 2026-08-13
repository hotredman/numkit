% matrix_factorizations.m
% 
% Demonstrates the standard matrix factorizations available in numkit:
% LU, QR, Cholesky, and Singular Value Decomposition (SVD).
% These operations are powered by optimized BLAS/LAPACK routines.

% Generate a random 4x4 matrix for demonstration
A = rand(4, 4);
disp('Original Matrix A:');
disp(A);

% 1. LU Factorization (A = P*L*U)
% LU factorization expresses a matrix as the product of a lower triangular 
% matrix (L) and an upper triangular matrix (U), with permutation matrix (P).
disp('--- LU Factorization ---');
[L, U, P] = lu(A);
disp('L (Lower triangular):');
disp(L);
disp('U (Upper triangular):');
disp(U);
disp('P (Permutation):');
disp(P);

% Verify the factorization: P*A should equal L*U
err_lu = norm(P*A - L*U);
fprintf('LU factorization error ||P*A - L*U||: %e\n\n', err_lu);


% 2. QR Factorization (A = Q*R)
% QR factorization expresses a matrix as the product of an orthogonal 
% matrix (Q) and an upper triangular matrix (R).
disp('--- QR Factorization ---');
[Q, R] = qr(A);
disp('Q (Orthogonal):');
disp(Q);
disp('R (Upper triangular):');
disp(R);

% Verify the factorization: Q*R should equal A
err_qr = norm(A - Q*R);
fprintf('QR factorization error ||A - Q*R||: %e\n\n', err_qr);


% 3. Singular Value Decomposition, SVD (A = U*S*V')
% SVD breaks a matrix into orthogonal matrices U and V, and a diagonal 
% matrix S containing the singular values.
disp('--- Singular Value Decomposition (SVD) ---');
[U_svd, S_svd, V_svd] = svd(A);
disp('Singular values (diagonal of S):');
disp(diag(S_svd));

% Verify the factorization: U*S*V' should equal A
err_svd = norm(A - U_svd * S_svd * V_svd');
fprintf('SVD factorization error ||A - U*S*V''||: %e\n\n', err_svd);


% 4. Cholesky Factorization (A = C'*C or A = R'*R)
% Cholesky factorization is highly efficient but only works for 
% Symmetric Positive-Definite (SPD) matrices.
disp('--- Cholesky Factorization ---');
% Create a symmetric positive-definite matrix
A_spd = A'*A + eye(4); 
disp('Symmetric Positive-Definite Matrix A_spd:');
disp(A_spd);

C = chol(A_spd);
disp('C (Upper triangular Cholesky factor):');
disp(C);

% Verify the factorization: C'*C should equal A_spd
err_chol = norm(A_spd - C'*C);
fprintf('Cholesky factorization error ||A_spd - C''*C||: %e\n', err_chol);
