import compat.*

% lognpdf(1; 0, 1) — log(1) = 0, so z=0 → 1/(x σ √(2π)) = 1/√(2π) = 0.398942
fprintf('lognpdf(1, 0, 1)    = %.6f  (expect 0.398942)\n', lognpdf(1, 0, 1));

% lognpdf(2; 0, 1)
% z = log(2)/1 = 0.6931, exp(-z²/2)/√(2π) = exp(-0.2402)/√(2π) = 0.7861/2.5066 = 0.3136
% pdf = 0.3136/2 = 0.1568
fprintf('lognpdf(2, 0, 1)    = %.6f  (expect 0.156874)\n', lognpdf(2, 0, 1));

% logncdf(1; 0, 1) = Φ(0) = 0.5
fprintf('logncdf(1, 0, 1)    = %.6f  (expect 0.500000)\n', logncdf(1, 0, 1));
% logncdf(e; 0, 1) = Φ(1) = 0.8413
fprintf('logncdf(exp(1), 0, 1) = %.6f  (expect 0.841345)\n', logncdf(exp(1), 0, 1));

% logninv: round-trip
fprintf('logninv(logncdf(2.5)) = %.6f  (expect 2.500000)\n', logninv(logncdf(2.5)));

% lognstat(0, 1): mean = e^0.5 ≈ 1.6487, var = (e - 1) * e ≈ 4.6708
[m, v] = lognstat(0, 1);
fprintf('lognstat(0, 1)      = [%.4f, %.4f]  (expect [1.6487, 4.6708])\n', m, v);

% rnd
N = 50000;
X = lognrnd(0, 1, N, 1);
fprintf('lognrnd(0,1) N=%d: mean=%.4f var=%.4f (expect 1.6487, 4.6708)\n', N, mean(X), var(X));
