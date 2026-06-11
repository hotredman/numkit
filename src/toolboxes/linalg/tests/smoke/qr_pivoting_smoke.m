clear
import compat.*

% Column-pivoting QR — [Q,R,P] = qr(A). Fixed 2026-06-05
% (bugs/linalg/qr-pivoting.md). Reference: MATLAB R2025b.
% Q signs may differ from MATLAB by a reflection; P / pivot order / |R| / the
% reconstruction A*P = Q*R all match.

A = [1 2; 3 4; 5 6];
[Q, R, P] = qr(A);
fprintf('3x2 P = [%g %g; %g %g]  (expect [0 1; 1 0])\n', P(1,1),P(1,2),P(2,1),P(2,2));
fprintf('    R(1,1) = %.6f (expect -7.483315)  recon err = %.2e\n', R(1,1), max(max(abs(A*P - Q*R))));

[~, ~, p] = qr(A, 'vector');
fprintf('    vector p = [%g %g]  (expect [2 1])\n', p(1), p(2));

B = [1 2 3; 4 5 6; 7 8 10];
[Qb, Rb, pb] = qr(B, 'vector');
fprintf('3x3 pivot order p = [%g %g %g]  (expect [3 1 2])\n', pb(1),pb(2),pb(3));
fprintf('    |diag(R)| = %.5f %.5f %.5f  (decreasing, rank-revealing)\n', abs(Rb(1,1)),abs(Rb(2,2)),abs(Rb(3,3)));
fprintf('    recon err = %.2e\n', max(max(abs(B(:,pb) - Qb*Rb))));

[Qe, Re, Pe] = qr(A, 0);
fprintf('econ 3-out: P is a vector size [%g %g] = [%g %g], Q is %gx%g\n', ...
        size(Pe,1), size(Pe,2), Pe(1), Pe(2), size(Qe,1), size(Qe,2));
