clear
import compat.*

% fmincon(fun, x0, A, b, Aeq, beq, lb, ub) — constrained minimization via an
% embedded-.m SQP that reuses quadprog as the QP subproblem (FD gradient,
% BFGS Hessian, backtracking line search). Parity with MATLAB is on the
% solution. Nonlinear constraints (nonlcon) are not supported (the VM cannot
% make the [c,ceq]=nonlcon(x) multi-output handle call). bugs/optim/fmincon.

% Bounds only: min x1^2+x2^2 s.t. 0<=x<=2 -> [0 0].
[x, fval, ef] = fmincon(@(x) x(1)^2+x(2)^2, [1 1], [],[],[],[], [0 0],[2 2]);
fprintf('bounds: [%.8f %.8f]  fval = %.6f  ef = %d  (expect [0 0] / 0 / 1)\n', ...
        x(1), x(2), fval, ef);

% Linear inequality x1+x2<=2 with a far objective center -> [1 1].
x = fmincon(@(x) (x(1)-2)^2+(x(2)-2)^2, [0 0], [1 1], 2);
fprintf('lin-ineq: [%.8f %.8f]  (expect [1 1])\n', x(1), x(2));

% Equality x1+x2=2 -> [1 1].
x = fmincon(@(x) x(1)^2+x(2)^2, [2 0], [],[], [1 1], 2);
fprintf('equality: [%.8f %.8f]  (expect [1 1])\n', x(1), x(2));

% Objective minimum outside the box clamps to the box corner -> [2 0].
x = fmincon(@(x) (x(1)-3)^2 + (x(2)+1)^2, [0 0], [],[],[],[], [0 0],[2 2]);
fprintf('bounded corner: [%.8f %.8f]  (expect [2 0])\n', x(1), x(2));
