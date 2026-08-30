clear

% Beta distribution smoke checks.
% Expected values cross-checked against textbook formulas / MATLAB R2025b.

% pdf(0.5; 2, 5): B(2,5) = 1/30, pdf = 0.5*0.5^4*30 = 0.9375
v1 = betapdf(0.5, 2, 5);
fprintf('betapdf(0.5, 2, 5) = %.6f  (expect 0.937500)\n', v1);

% pdf(0.3; 2, 5)
% B(2,5)=1/30, pdf = 30 * 0.3 * 0.7^4 = 30 * 0.3 * 0.2401 = 2.16090
v2 = betapdf(0.3, 2, 5);
fprintf('betapdf(0.3, 2, 5) = %.6f  (expect 2.160900)\n', v2);

% cdf(0.5; 2, 5)
v3 = betacdf(0.5, 2, 5);
fprintf('betacdf(0.5, 2, 5) = %.6f  (expect 0.890625)\n', v3);

% cdf(1.0; 2, 5) = 1
v4 = betacdf(1.0, 2, 5);
fprintf('betacdf(1.0, 2, 5) = %.6f  (expect 1.000000)\n', v4);

% inv: round-trip
v5 = betainv(betacdf(0.4, 3, 5), 3, 5);
fprintf('betainv(betacdf(0.4)) = %.6f  (expect 0.400000)\n', v5);

% Symmetric: betainv(0.5, 2, 2) = 0.5
v6 = betainv(0.5, 2, 2);
fprintf('betainv(0.5, 2, 2) = %.6f  (expect 0.500000)\n', v6);

% stat
[m, v] = betastat(2, 5);
fprintf('betastat(2, 5) = [%.6f, %.6f]  (expect [0.285714, 0.025510])\n', m, v);

% rnd statistics
N = 50000;
X = betarnd(2, 5, N, 1);
fprintf('betarnd(2,5) N=%d: mean=%.4f var=%.4f (expect 0.2857, 0.0255)\n', ...
    N, mean(X), var(X));
