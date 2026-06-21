clear
import compat.*

% Real Schur of a nonsymmetric matrix -- bugs/linalg/schur-nonsymmetric.
% Hessenberg reduction + Francis double-shift QR + 2x2 block standardization.
% A = U*T*U', U orthogonal, T quasi-upper-triangular (the form is not unique;
% eigenvalues + reconstruction are the invariants).

A = [1 2 3; 4 5 6; 7 8 10];
[U, T] = schur(A);
fprintf('real-eig A:\n');
fprintf('  eig(diag T) = %s   (expect -0.9057 0.1981 16.7077)\n', num2str(sort(diag(T))', '%.4f '));
fprintf('  reconstruction err = %.2e\n', max(abs(A(:) - reshape(U*T*U', [], 1))));
fprintf('  orthogonality  err = %.2e\n', max(abs(reshape(U'*U - eye(3), [], 1))));
fprintf('  strictly-below-diag = %.2e  (expect ~0: triangular for real eig)\n', ...
        max([abs(T(2,1)) abs(T(3,1)) abs(T(3,2))]));

% complex-conjugate pair -> a 2x2 block is retained
B = [0 1 0 0; 0 0 0 -1; -1 0 0 0; 0 -1 -1 0];
[V, S] = schur(B);
fprintf('complex-pair B: recon=%.2e  orth=%.2e\n', ...
        max(abs(B(:) - reshape(V*S*V', [], 1))), max(abs(reshape(V'*V - eye(4), [], 1))));
