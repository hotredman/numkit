clear

% Left eigenvectors W — [V,D,W] = eig(A). Fixed 2026-06-05
% (bugs/linalg/eig-left-vectors.md). Reference: MATLAB R2025b.
% W column signs may differ from MATLAB; the relation W'*A = D*W', unit-norm
% columns, and sum(abs(W)) all match.

A = [4 -2; 1 1];
[V, D, W] = eig(A);
fprintf('nonsym 2x2: relation err = %.2e   col norms = %.4f %.4f\n', ...
        max(max(abs(W'*A - D*W'))), norm(W(:,1)), norm(W(:,2)));
fprintf('            sum(abs(W)) = %.6f  (expect 2.755854)\n', sum(sum(abs(W))));

B = [2 1; 1 3];
[Vb, Db, Wb] = eig(B);
fprintf('symmetric : W == V?  maxdiff = %.2e   |W(1,1)| = %.6f (expect 0.850651)\n', ...
        max(max(abs(Wb - Vb))), abs(Wb(1,1)));

N = [2 0 0; 1 3 0; 0 1 4];
[Vn, Dn, Wn] = eig(N);
fprintf('nonsym 3x3: relation err = %.2e   sum(abs(W)) = %.6f  (expect 4.080880)\n', ...
        max(max(abs(Wn'*N - Dn*Wn'))), sum(sum(abs(Wn))));
% biorthogonality: W'*V is diagonal (different eigenvalues are orthogonal)
G = Wn' * Vn;
fprintf('            W''*V off-diagonal max = %.2e  (expect ~0)\n', ...
        max(abs([G(1,2) G(1,3) G(2,3)])));
