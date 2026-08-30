clear

% binopdf: C(n,k) p^k (1-p)^(n-k)
% binopdf(2, 5, 0.4) = C(5,2) * 0.16 * 0.216 = 10 * 0.034560 = 0.34560
fprintf('binopdf(2, 5, 0.4)  = %.6f  (expect 0.345600)\n', binopdf(2, 5, 0.4));
% binopdf(0, 10, 0.1) = 0.9^10 = 0.348678
fprintf('binopdf(0, 10, 0.1) = %.6f  (expect 0.348678)\n', binopdf(0, 10, 0.1));

% binocdf
% binocdf(2, 5, 0.4): F(0)+F(1)+F(2) = 0.07776+0.2592+0.3456 = 0.68256
fprintf('binocdf(2, 5, 0.4)  = %.6f  (expect 0.682560)\n', binocdf(2, 5, 0.4));
fprintf('binocdf(5, 5, 0.4)  = %.6f  (expect 1.000000)\n', binocdf(5, 5, 0.4));

% binoinv
% binoinv(0.5, 10, 0.5) = 5
fprintf('binoinv(0.5, 10, 0.5) = %.6f  (expect 5)\n', binoinv(0.5, 10, 0.5));
% binoinv(0.95, 20, 0.3) = 9 (MATLAB)
fprintf('binoinv(0.95, 20, 0.3) = %.6f  (expect 9)\n', binoinv(0.95, 20, 0.3));
% Round-trip
fprintf('binoinv(binocdf(3, 8, 0.5), 8, 0.5) = %.6f  (expect 3)\n', ...
    binoinv(binocdf(3, 8, 0.5), 8, 0.5));

% binostat
[m, v] = binostat(20, 0.3);
fprintf('binostat(20, 0.3)   = [%.4f, %.4f]  (expect [6, 4.2])\n', m, v);

% rnd
N = 50000;
X = binornd(20, 0.3, N, 1);
fprintf('binornd(20, 0.3) N=%d: mean=%.4f var=%.4f (expect 6, 4.2)\n', N, mean(X), var(X));
