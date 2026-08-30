clear

% Standard normal sample → kstest should accept H0 (normal)
rng(42);
x = randn(200, 1);
[h, p, D, cv] = kstest(x);
fprintf('--- kstest(N(0,1) sample) ---\n');
fprintf('h = %d, p = %.4f, D = %.4f, cv = %.4f\n', h, p, D, cv);
fprintf('  expect: h=0 (cannot reject normality), p > 0.05\n\n');

% Uniform sample → kstest should reject normality
y = (rand(200, 1) - 0.5) * 4;   % U(-2, 2)
[h2, p2] = kstest(y);
fprintf('--- kstest(uniform sample) ---\n');
fprintf('h = %d, p = %.4f\n', h2, p2);
fprintf('  expect: h=1, very small p\n\n');

% kstest2 — same distribution
a = randn(100, 1);
b = randn(100, 1);
[h3, p3] = kstest2(a, b);
fprintf('--- kstest2(N(0,1), N(0,1)) ---\n');
fprintf('h = %d, p = %.4f\n', h3, p3);
fprintf('  expect: h=0, p large\n\n');

% kstest2 — different scale
c = randn(100, 1) * 3 + 2;   % N(2, 9)
[h4, p4, D4] = kstest2(a, c);
fprintf('--- kstest2(N(0,1), N(2, 9)) ---\n');
fprintf('h = %d, p = %.6f, D = %.4f\n', h4, p4, D4);
fprintf('  expect: h=1, very small p\n\n');

% jbtest on normal sample
[h5, p5, JB5] = jbtest(x);
fprintf('--- jbtest(normal) ---\n');
fprintf('h = %d, p = %.4f, JB = %.4f\n', h5, p5, JB5);
fprintf('  expect: h=0, p large (JB ~ χ²(2))\n\n');

% jbtest on heavy-tailed (Student's t with low df)
rng(99);
t_samp = trnd(2, 200, 1);  % t(2) has very heavy tails
[h6, p6, JB6] = jbtest(t_samp);
fprintf('--- jbtest(t(2) sample) ---\n');
fprintf('h = %d, p = %.6f, JB = %.4f\n', h6, p6, JB6);
fprintf('  expect: h=1, large JB (rejects normality)\n');
