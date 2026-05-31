clear
import compat.*

% svd 'econ', qr 'econ', lu 'vector'.
A = [1 2; 3 4; 5 6];   % 3x2 tall

% svd economy: U 3x2, S 2x2, V 2x2.
[Ue, Se, Ve] = svd(A, 'econ');
fprintf('svd econ : U %dx%d S %dx%d V %dx%d  sv=%.6f %.6f\n', ...
        size(Ue,1), size(Ue,2), size(Se,1), size(Se,2), size(Ve,1), size(Ve,2), Se(1,1), Se(2,2));
fprintf('  recon  : %.2e\n', max(max(abs(Ue*Se*Ve' - A))));   % expect ~0

% qr economy: Q 3x2, R 2x2.
[Qe, Re] = qr(A, 'econ');
fprintf('qr econ  : Q %dx%d R %dx%d  recon %.2e\n', ...
        size(Qe,1), size(Qe,2), size(Re,1), size(Re,2), max(max(abs(Qe*Re - A))));

% lu vector: p is a row index vector, A(p,:) = L*U.
M = [4 3; 6 3];
[L, U, p] = lu(M, 'vector');
fprintf('lu vector: p=%s  recon %.2e\n', mat2str(p(:).'), max(max(abs(M(p,:) - L*U))));  % p=[2 1]
