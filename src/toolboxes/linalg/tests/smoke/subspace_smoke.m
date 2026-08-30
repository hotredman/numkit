clear

fprintf('=== subspace (angle between subspaces) ===\n');

A = [1 0; 0 1; 0 0];
fprintf('  subspace(A, A) = %g (expect 0)\n', subspace(A, A));

B = [1 0; 0 0; 0 1];
fprintf('  subspace(A, B) (share x-axis, second axis differs) = %g\n', subspace(A, B));

% Orthogonal column spaces
C = [0 0; 0 0; 1 0];   % spans z-axis (1D)
D = [1; 0; 0];         % spans x-axis (1D)
fprintf('  subspace([0 0; 0 0; 1 0], [1; 0; 0]) = %g (expect pi/2 = %g)\n', ...
        subspace(C, D), pi/2);
