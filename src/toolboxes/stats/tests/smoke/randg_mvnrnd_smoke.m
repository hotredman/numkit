clear
import compat.*

fprintf('=== randg — raw gamma(shape, 1) RNG ===\n');
rng(0);

% Scalar shape.
fprintf('\nrandg(2.5) (single sample): %g\n', randg(2.5));

% Matrix size.
R = randg(3.0, 2000, 1);
fprintf('\nrandg(3.0, 2000, 1):\n');
fprintf('   sample mean = %.4f  (theoretical: 3.0)\n', mean(R));
fprintf('   sample var  = %.4f  (theoretical: 3.0)\n', var(R));

% Per-element shape.
fprintf('\nrandg([1; 2; 5; 10]) (per-element shape):\n');
disp(randg([1; 2; 5; 10]));


fprintf('\n=== mvnrnd — multivariate normal RNG ===\n');

mu = [1, 2, 3];
Sigma = [4 1 0; 1 9 0; 0 0 16];

% n samples.
R = mvnrnd(mu, Sigma, 5000);
fprintf('\nmvnrnd([1 2 3], [4 1 0; 1 9 0; 0 0 16], 5000):\n');
fprintf('   sample mean ='); disp(mean(R));
fprintf('   sample cov  =\n'); disp(cov(R));

% Single sample.
fprintf('mvnrnd(mu, Sigma) (one sample):\n');
disp(mvnrnd(mu, Sigma));

% Per-row mu (one sample per row, two clusters).
fprintf('\nPer-row mu — 200 samples around (0,0), 200 around (5,5):\n');
MU = [zeros(200, 2); 5 * ones(200, 2)];
RR = mvnrnd(MU, eye(2));
fprintf('   cluster 1 mean = '); disp(mean(RR(1:200, :)));
fprintf('   cluster 2 mean = '); disp(mean(RR(201:400, :)));
