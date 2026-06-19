clear
import compat.*

% linprog(f, A, b, Aeq, beq, lb, ub) — minimise f'x subject to A*x<=b,
% Aeq*x=beq, lb<=x<=ub. Solved by proximal regularization over quadprog
% (exact vertex for a unique optimum). bugs/optim/linprog.

% Lower-bound LP: x1>=1, x2>=1, min x1+x2 -> [1 1].
[x, fval, ef] = linprog([1 1], [-1 0; 0 -1], [-1; -1]);
fprintf('lower-bound: [%.8f %.8f]  fval = %.6f  ef = %d  (expect [1 1] / 2 / 1)\n', ...
        x(1), x(2), fval, ef);

% Classic: max 3x+2y s.t. x+y<=4, x+3y<=6, x>=0 (= min -3x-2y) -> [4 0].
[x, fval] = linprog([-3 -2], [1 1; 1 3], [4; 6], [], [], [0 0], []);
fprintf('classic max: [%.8f %.8f]  fval = %.6f  (expect [4 0] / -12)\n', x(1), x(2), fval);

% min -x1-2x2 s.t. x1+x2<=4, x1<=3, x>=0 -> [0 4].
x = linprog([-1 -2], [1 1; 1 0], [4; 3], [], [], [0 0], []);
fprintf('bounded vertex: [%.8f %.8f]  (expect [0 4])\n', x(1), x(2));

% Box bounds only: min -x1-x2 s.t. 0<=x<=[2 3] -> [2 3].
x = linprog([-1 -1], [], [], [], [], [0 0], [2 3]);
fprintf('box bounds: [%.8f %.8f]  (expect [2 3])\n', x(1), x(2));
