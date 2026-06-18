clear
import compat.*

% Algebraic Riccati solvers via the matrix sign-function method.
% bugs/control/care-dare. Outputs match MATLAB's [X, L, G]:
%   X = stabilizing solution, L = closed-loop eigenvalues, G = gain.

% --- care: continuous-time ---------------------------------------------
% A'X + XA - XBR^-1B'X + Q = 0
A = [0 1; 0 0]; B = [0; 1]; Q = eye(2);
[X, L, G] = care(A, B, Q);
fprintf('care X(1,1) = %.12f   (expect 1.732050807569 = sqrt(3))\n', X(1,1));
fprintf('care trace  = %.12f   (expect 3.464101615138)\n', trace(X));
fprintf('care gain G = [%.6f %.6f]   (expect 1.000000 1.732051)\n', G(1), G(2));
fprintf('care poles  = %.4f %+.4fi , %.4f %+.4fi  (expect -0.8660 +-0.5000i)\n', ...
        real(L(1)), imag(L(1)), real(L(2)), imag(L(2)));
% residual of the continuous ARE
res_c = A'*X + X*A - X*B*(B'*X) + Q;
fprintf('care residual norm = %.2e\n', max(abs(res_c(:))));

% second case (A2=[-3 2;1 1], B2=[0;1], Q2=diag([1 2]), R2=3)
A2 = [-3 2; 1 1]; B2 = [0; 1]; Q2 = [1 0; 0 2]; R2 = 3;
X2 = care(A2, B2, Q2, R2);
fprintf('care2 trace = %.8f   (expect 9.92682542)\n', trace(X2));

% --- dare: discrete-time -----------------------------------------------
% A'XA - X - A'XB(R+B'XB)^-1B'XA + Q = 0
Ad = [1 1; 0 1]; Bd = [0; 1]; Qd = eye(2); Rd = 1;
[Xd, Ld, Gd] = dare(Ad, Bd, Qd, Rd);
fprintf('dare X(1,1) = %.8f   (expect 2.94712297)\n', Xd(1,1));
fprintf('dare trace  = %.8f   (expect 7.56025723)\n', trace(Xd));
fprintf('dare |poles| = %.4f %.4f   (expect <1, stable: ~0.4221)\n', abs(Ld(1)), abs(Ld(2)));
res_d = Ad'*Xd*Ad - Xd - (Ad'*Xd*Bd)*((Rd + Bd'*Xd*Bd)\(Bd'*Xd*Ad)) + Qd;
fprintf('dare residual norm = %.2e\n', max(abs(res_d(:))));
