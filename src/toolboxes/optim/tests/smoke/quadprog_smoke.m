clear

% quadprog(H, f, A, b, Aeq, beq, lb, ub) — minimise 0.5*x'Hx + f'x subject to
% A*x<=b, Aeq*x=beq, lb<=x<=ub. Embedded-.m primal active-set for a strictly
% convex (PD) H. bugs/optim/quadprog.

% Unconstrained: min 0.5||x||^2 - x1 - x2 -> [1 1].
x = quadprog(eye(2), [-1 -1]);
fprintf('unconstrained: [%.8f %.8f]  (expect [1 1])\n', x(1), x(2));

% Inequality x1 + x2 <= 1 -> [0.5 0.5].
[x, fval] = quadprog(eye(2), [-1 -1], [1 1], 1);
fprintf('ineq x1+x2<=1: [%.8f %.8f]  fval = %.8f  (expect [0.5 0.5] / -0.75)\n', ...
        x(1), x(2), fval);

% Equality x1 + x2 = 3 -> [1.5 1.5].
x = quadprog(eye(2), [-1 -1], [], [], [1 1], 3);
fprintf('eq x1+x2=3: [%.8f %.8f]  (expect [1.5 1.5])\n', x(1), x(2));

% Bounds 0 <= x <= 0.3 -> [0.3 0.3].
x = quadprog(eye(2), [-1 -1], [], [], [], [], [0 0], [0.3 0.3]);
fprintf('bounds [0,0.3]: [%.8f %.8f]  (expect [0.3 0.3])\n', x(1), x(2));

% Non-identity H with an active inequality.
x = quadprog([2 0; 0 4], [-2; -8], [1 1], 1);
fprintf('mixed H: [%.8f %.8f]  (expect [-0.3333 1.3333])\n', x(1), x(2));

% Two active inequalities.
x = quadprog(eye(2), [-2 -2], [1 1; 1 0], [1; 0.3]);
fprintf('two ineq: [%.8f %.8f]  (expect [0.3 0.7])\n', x(1), x(2));
