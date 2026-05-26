clear
import compat.*
rng(0);

fprintf('=== mvtrnd — multivariate t RNG ===\n');
C = [1 0.5; 0.5 1];
df = 5;
R = mvtrnd(C, df, 5000);
fprintf('sample-cov · (df-2)/df:\n');
disp(cov(R) * (df - 2) / df);
fprintf('true correlation C:\n');
disp(C);

fprintf('\n=== mnrnd — multinomial RNG ===\n');
p = [0.2, 0.3, 0.5];
N = 100;
M = mnrnd(N, p, 1000);
fprintf('Per-column means (1000 samples, N=100 trials):\n');
disp(mean(M));
fprintf('Expected: [%.0f, %.0f, %.0f]\n', N*p(1), N*p(2), N*p(3));
fprintf('All row sums = %d (each row sums to N)\n', unique(sum(M, 2)));
