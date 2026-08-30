clear

fprintf('=== mvncdf — multivariate normal CDF ===\n');

fprintf('\n1-D (forwards to normcdf):\n');
fprintf('  mvncdf(0)   = %.4f  (ref: 0.5000)\n',    mvncdf(0,   [], []));
fprintf('  mvncdf(1.5) = %.4f  (ref: 0.9332)\n',    mvncdf(1.5, [], []));

fprintf('\n2-D independent (Σ = I):\n');
fprintf('  P([X1≤0, X2≤0]) = %.4f  (ref: 0.25)\n', mvncdf([0 0], [], eye(2)));
fprintf('  P([1.96,1.96])  = %.4f  (ref ≈ 0.951)\n', mvncdf([1.96 1.96], [], eye(2)));

fprintf('\n2-D correlated (ρ = 0.5):\n');
S = [1 0.5; 0.5 1];
fprintf('  P at origin     = %.4f  (ref: 1/4 + asin(0.5)/(2π) = 0.3333)\n', ...
        mvncdf([0 0], [0 0], S));

fprintf('\n3-D independent (Monte Carlo):\n');
fprintf('  P([0 0 0])     = %.4f  (ref: 0.125)\n', mvncdf([0 0 0], [], eye(3)));

fprintf('\nMultiple queries:\n');
X = [0 0; 1 1; -1 -1];
P = mvncdf(X, [0 0], eye(2));
fprintf('  P = '); disp(P');
