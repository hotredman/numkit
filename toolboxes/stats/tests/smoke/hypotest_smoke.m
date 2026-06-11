clear

import compat.*

% ttest — sample mean vs known μ=0
x = [1 2 3 4 5 6 7 8 9 10];   % mean = 5.5
[h, p, ci, t] = ttest(x);
fprintf('--- ttest(x) vs μ=0 ---\n');
fprintf('h = %d, p = %.6f, ci = [%.4f, %.4f], t = %.4f\n', h, p, ci(1), ci(2), t);
% n=10, mean=5.5, var=9.1667, se=0.957, t = 5.5/0.957 = 5.745
% t.cdf(5.745, 9) ≈ 0.9999 → p = 2*(1 - 0.9999) ≈ 1.4e-4
fprintf('  expect: h=1 (reject), p ≈ 0.00027, t ≈ 5.745\n\n');

% ttest with explicit μ that matches sample
[h2, p2] = ttest(x, 5.5);
fprintf('--- ttest(x, 5.5) ---\n');
fprintf('h = %d, p = %.4f\n', h2, p2);
fprintf('  expect: h=0 (cannot reject), p ≈ 1.0\n\n');

% ttest2 — two samples with same mean
y = x + 0.1;
[h3, p3, ~, t3] = ttest2(x, y);
fprintf('--- ttest2(x, y) where y = x + 0.1 ---\n');
fprintf('h = %d, p = %.4f, t = %.4f\n', h3, p3, t3);
fprintf('  expect: h=0 (means very close), small |t|\n\n');

% ttest2 — clearly different means
z = x + 10;
[h4, p4, ~, t4] = ttest2(x, z);
fprintf('--- ttest2(x, z) where z = x + 10 ---\n');
fprintf('h = %d, p = %.6f, t = %.4f\n', h4, p4, t4);
fprintf('  expect: h=1, very small p, large |t|\n\n');

% ztest with σ=3
[h5, p5, ~, z5] = ztest(x, 0, 3);
fprintf('--- ztest(x, 0, 3) ---\n');
fprintf('h = %d, p = %.6f, z = %.4f\n', h5, p5, z5);
% z = 5.5 / (3/sqrt(10)) = 5.5/0.949 = 5.798
fprintf('  expect: h=1, z ≈ 5.798, p ≈ 7e-9\n\n');

% vartest
[h6, p6, ~, T6] = vartest(x, 5);
fprintf('--- vartest(x, 5) where var(x) ≈ 9.17 ---\n');
fprintf('h = %d, p = %.4f, T = %.4f\n', h6, p6, T6);
% T = 9 * 9.1667 / 5 = 16.5
fprintf('  expect: h=0 or 1 (depends on threshold), T ≈ 16.5\n\n');

% vartest2 — same data, different var
y2 = [1 2 3 4 5 6 7 8 9 10] * 2;   % var = 4*var(x) = 36.67
[h7, p7, ~, F7] = vartest2(x, y2);
fprintf('--- vartest2(x, y2) where var(y2) = 4*var(x) ---\n');
fprintf('h = %d, p = %.4f, F = %.4f\n', h7, p7, F7);
fprintf('  expect: h=1, F ≈ 0.25, p ≈ 0.06 (two-sided)\n');
