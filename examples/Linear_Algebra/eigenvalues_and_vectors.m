% eigenvalues_and_vectors.m
% 
% Demonstrates how to compute eigenvalues and eigenvectors of matrices
% using the eig() function.

disp('--- Eigenvalues and Eigenvectors ---');

% Create a square matrix
A = [2, 1, 1; 
     1, 3, 2; 
     1, 2, 2];
disp('Matrix A:');
disp(A);

% 1. Compute just the eigenvalues
% If only one output is requested, eig() returns a vector of eigenvalues
eigenvalues = eig(A);
disp('Eigenvalues of A:');
disp(eigenvalues);

% 2. Compute eigenvalues and eigenvectors
% If two outputs are requested, eig() returns a matrix of eigenvectors (V)
% and a diagonal matrix of eigenvalues (D)
[V, D] = eig(A);
disp('Eigenvectors (columns of V):');
disp(V);
disp('Eigenvalues (diagonal of D):');
disp(D);

% 3. Verify the eigendecomposition
% For the eigendecomposition A*V = V*D
% Let's verify this holds true:
AV = A * V;
VD = V * D;
err_eig = norm(AV - VD);
fprintf('Eigendecomposition error ||A*V - V*D||: %e\n\n', err_eig);

% 4. Symmetric matrices
% Symmetric matrices always have real eigenvalues and orthogonal eigenvectors.
disp('--- Properties of Symmetric Matrices ---');
% Verify that eigenvectors of our symmetric matrix A are orthogonal (V*V' approx I)
orthogonality_err = norm(V * V' - eye(3));
fprintf('Orthogonality error ||V*V'' - I||: %e\n', orthogonality_err);

% 5. Non-symmetric matrices (Complex eigenvalues)
% Non-symmetric matrices can have complex eigenvalues and eigenvectors.
disp('--- Non-Symmetric Matrices ---');
B = [0, -1; 1, 0]; % A 90-degree rotation matrix
disp('Matrix B (Rotation by 90 degrees):');
disp(B);

eigenvalues_B = eig(B);
disp('Eigenvalues of B (Complex):');
disp(eigenvalues_B);
