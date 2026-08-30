clear

rng(0);
X = lhsdesign(5, 3);
fprintf('lhsdesign(5, 3):\n');
disp(X);
fprintf('All in (0,1): %d\n', all(X(:) > 0 & X(:) < 1));
for j = 1:3
    bins = ceil(X(:, j) * 5);
    fprintf('col %d sorted bins: ', j);
    disp(sort(bins'));
end

% 1000-sample mean ≈ 0.5
rng(0);
X = lhsdesign(1000, 2);
fprintf('\nlhsdesign(1000, 2) column means: '); disp(mean(X));
fprintf('  (expect ~ [0.5 0.5])\n');

% lhsnorm
mu = [10 -5];
Sigma = [1 0.5; 0.5 1];
rng(0);
Y = lhsnorm(mu, Sigma, 1000);
fprintf('\nlhsnorm(mu=[10 -5], Sigma=[1,0.5;0.5,1], 1000):\n');
fprintf('  sample mean: '); disp(mean(Y));
fprintf('  expect: [10 -5]\n');
fprintf('  sample cov:\n'); disp(cov(Y));
fprintf('  expect: [1 0.5; 0.5 1]\n');
