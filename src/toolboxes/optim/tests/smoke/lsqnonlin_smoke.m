clear

% lsqnonlin(fun, p0) — minimise ‖F(p)‖² via embedded-.m Levenberg-Marquardt.
% lsqcurvefit(fun, p0, x, y) = lsqnonlin(@(p) fun(p,x)-y, p0). Parity with
% MATLAB is on the solution. bugs/optim/nonlinear-lsq.

% Exact linear residual -> [1 2], resnorm 0.
[p, resnorm, residual, ef] = lsqnonlin(@(p) [p(1)-1; p(2)-2], [0 0]);
fprintf('lsqnonlin linear: p = [%.10f %.10f]  resnorm = %.2e  ef = %d  (expect [1 2] / 0 / 1)\n', ...
        p(1), p(2), resnorm, ef);

% Rosenbrock residual -> [1 1].
pr = lsqnonlin(@(x) [10*(x(2)-x(1)^2); 1-x(1)], [-1.2 1]);
fprintf('lsqnonlin rosenbrock: p = [%.8f %.8f]  (expect [1 1])\n', pr(1), pr(2));

% lsqcurvefit exp fit: flat valley -> resnorm is the tight quantity.
[pe, rne] = lsqcurvefit(@(p,xd) p(1)*exp(p(2)*xd), [1 -1], [0 1 2], [1 0.5 0.2]);
fprintf('lsqcurvefit exp: p = [%.6f %.6f]  resnorm = %.12f  (expect resnorm 0.001248164767)\n', ...
        pe(1), pe(2), rne);

% lsqcurvefit recovers exact params from noise-free data: 2*sin(1.5*x).
xd = [0 0.5 1 1.5 2]; yd = 2*sin(1.5*xd);
ps = lsqcurvefit(@(p,x) p(1)*sin(p(2)*x), [1 1], xd, yd);
fprintf('lsqcurvefit sin: p = [%.8f %.8f]  (expect [2 1.5])\n', ps(1), ps(2));

% lsqcurvefit == lsqnonlin on the equivalent residual.
a = lsqcurvefit(@(p,x) p(1)*exp(p(2)*x), [2 -0.5], [0 1 2 3], [2 1.2 0.7 0.45]);
b = lsqnonlin(@(p) p(1)*exp(p(2)*[0 1 2 3]) - [2 1.2 0.7 0.45], [2 -0.5]);
fprintf('lsqcurvefit == lsqnonlin: max diff = %.2e  (expect ~0)\n', max(abs(a - b)));
