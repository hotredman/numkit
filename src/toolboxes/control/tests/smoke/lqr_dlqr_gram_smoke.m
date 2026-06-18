clear
import compat.*

% LQR / DLQR optimal control gains + controllability/observability gramians.
% bugs/control/lqr-hinfnorm. lqr/dlqr wrap care/dare; gram wraps lyap/dlyap.
% MATLAB output order [K, S, P]: gain, Riccati solution, closed-loop poles.

% --- lqr (continuous) --------------------------------------------------
A = [0 1; 0 0]; B = [0; 1]; Q = eye(2); R = 1;
[K, S, P] = lqr(A, B, Q, R);
fprintf('lqr K = [%.6f %.6f]   (expect 1.000000 1.732051)\n', K(1), K(2));
fprintf('lqr S(1,1) = %.6f   (expect 1.732051 = care X)\n', S(1,1));
fprintf('lqr poles  = %.4f %+.4fi  (expect -0.8660 +-0.5000i)\n', real(P(1)), imag(P(1)));

% --- dlqr (discrete) ---------------------------------------------------
Ad = [0.9 0.1; 0 0.8]; Bd = [0; 1];
Kd = dlqr(Ad, Bd, eye(2), 1);
fprintf('dlqr K = [%.6f %.6f]   sum = %.6f   (expect sum 0.710044)\n', Kd(1), Kd(2), sum(Kd));

% --- gram (controllability / observability) ----------------------------
sys = ss([-1 0; 0 -2], [1; 1], [1 1], 0);
Wc = gram(sys, 'c');
Wo = gram(sys, 'o');
fprintf('gram c sum = %.6f   (expect 1.416667)\n', sum(Wc(:)));
fprintf('gram c = [%.4f %.4f; %.4f %.4f]   (expect [0.5 0.3333; 0.3333 0.25])\n', ...
        Wc(1,1), Wc(1,2), Wc(2,1), Wc(2,2));
fprintf('gram o sum = %.6f   (expect 1.416667)\n', sum(Wo(:)));
% controllability gramian solves A*Wc + Wc*A' + B*B' = 0
resc = [-1 0; 0 -2]*Wc + Wc*[-1 0; 0 -2]' + [1;1]*[1;1]';
fprintf('gram c residual = %.2e\n', max(abs(resc(:))));
