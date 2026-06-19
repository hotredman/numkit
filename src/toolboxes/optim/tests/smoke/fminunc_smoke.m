clear
import compat.*

% fminunc(fun, x0) — unconstrained minimization via embedded-.m BFGS
% quasi-Newton (central-difference gradient, Armijo line search). Like
% fminsearch but gradient-based. Parity with MATLAB is on the solution.
% bugs/optim/fminunc.

% Scalar parabola.
x = fminunc(@(x) (x-3)^2, 0);
fprintf('parabola (x-3)^2: x = %.10f  (expect 3)\n', x);

% 2-D quadratic bowl -> [1 -2], floor 3.
[xb, fval, ef] = fminunc(@(x) (x(1)-1)^2 + 2*(x(2)+2)^2 + 3, [0 0]);
fprintf('quad bowl: x = [%.8f %.8f]  fval = %.8f  ef = %d  (expect [1 -2] / 3 / 1)\n', ...
        xb(1), xb(2), fval, ef);

% Rosenbrock -> [1 1].
xr = fminunc(@(x) 100*(x(2)-x(1)^2)^2 + (1-x(1))^2, [-1.2 1]);
fprintf('rosenbrock: x = [%.6f %.6f]  (expect [1 1])\n', xr(1), xr(2));

% Column x0 -> column minimiser.
xc = fminunc(@(x) (x(1)-1)^2 + 2*(x(2)+2)^2, [0; 0]);
fprintf('column x0: size = [%d %d]  (expect [2 1])\n', size(xc,1), size(xc,2));
