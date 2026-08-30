clear

% fsolve(fun, x0) — solve a system of nonlinear equations F(x)=0 via an
% embedded-.m Levenberg-Marquardt iteration (forward-difference Jacobian).
% Parity with MATLAB is on the solution (the root). bugs/optim/fsolve.

% Scalar: x^2 - 2 = 0 -> sqrt(2).
x = fsolve(@(x) x^2 - 2, 1);
fprintf('scalar x^2-2=0: x = %.12f  (expect 1.414213562373)\n', x);

% 2x2 system on the unit circle with x1 = x2.
[xv, fval, ef] = fsolve(@(v) [v(1)^2+v(2)^2-1; v(1)-v(2)], [0.5 0.5]);
fprintf('2x2 system: x = [%.10f %.10f]  (expect [0.7071 0.7071])\n', xv(1), xv(2));
fprintf('  exitflag = %d, |F(x)| = %.2e  (expect 1, ~0)\n', ef, norm(fval));

% Rosenbrock-style square system, root [1 1].
xr = fsolve(@(x) [10*(x(2)-x(1)^2); 1-x(1)], [-1.2 1]);
fprintf('rosenbrock system: x = [%.8f %.8f]  (expect [1 1])\n', xr(1), xr(2));

% 3-variable system (roots are permutations of [1 2 3]); from [1 0 4] -> [1 2 3].
F = @(x) [x(1)+x(2)+x(3)-6; x(1)^2+x(2)^2+x(3)^2-14; x(1)*x(2)*x(3)-6];
x3 = fsolve(F, [1 0 4]);
fprintf('3-var system: x = [%.6f %.6f %.6f]  (expect [1 2 3])\n', x3(1), x3(2), x3(3));

% Column x0 -> column root.
xc = fsolve(@(v) [v(1)^2+v(2)^2-1; v(1)-v(2)], [0.5; 0.5]);
fprintf('column x0: size = [%d %d]  (expect [2 1])\n', size(xc,1), size(xc,2));
