clear
import compat.*

% polyfit [p,S,mu] centering + polyval [y,delta] error estimate.
x = [1 2 3 4 5];
y = [2.1 3.9 6.2 7.8 10.1];

% Plain coefficients (uncentered).
p = polyfit(x, y, 1);
fprintf('p        : %.6f %.6f\n', p(1), p(2));            % expect 1.990000 0.050000

% Centered fit with mu = [mean; std(N-1)].
[pc, S, mu] = polyfit(x, y, 1);
fprintf('mu       : %.6f %.6f\n', mu(1), mu(2));          % expect 3.000000 1.581139
fprintf('pc       : %.6f %.6f\n', pc(1), pc(2));          % expect 3.146466 6.020000
fprintf('S.normr  : %.6f\n', S.normr);                    % expect 0.327109
fprintf('S.df     : %g\n', S.df);                         % expect 3

% Prediction + 1-sigma-ish error estimate at x = 3.
[yhat, delta] = polyval(pc, 3, S, mu);
fprintf('yhat     : %.6f\n', yhat);                       % expect 6.020000
fprintf('delta    : %.6f\n', delta);                      % expect 0.206882

% [p,S] without mu does not center.
[p2, S2] = polyfit(x, y, 1);
fprintf('p2 (raw) : %.6f %.6f  df=%g\n', p2(1), p2(2), S2.df); % expect 1.99 0.05 df=3
