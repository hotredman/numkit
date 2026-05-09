clear
import compat.*

fprintf('=== crosstab (contingency table + chi-square) ===\n');

x = [1 1 2 2 3 3 1 2 3];
y = [10 20 10 20 10 20 20 10 20];
[T, chi2, p] = crosstab(x, y);
fprintf('  crosstab(x, y):\n');
disp(T)
fprintf('  expect:\n     1     2\n     2     1\n     1     2\n');
fprintf('  chi2 = %.6f (expect 0.900000)\n', chi2);
fprintf('  p    = %.6f (expect 0.637628)\n', p);

[T2, ~, ~] = crosstab([1 2 1 3 2 1]);
fprintf('\n  single-arg crosstab([1 2 1 3 2 1]):\n');
disp(T2)
fprintf('  expect column [3; 2; 1]\n');

% Independence — well-mixed
[T3, c3, p3] = crosstab([1 1 2 2 1 1 2 2], [1 2 1 2 1 2 1 2]);
fprintf('\n  independent crosstab: chi2=%.6f, p=%.6f (expect chi2≈0, p≈1)\n', c3, p3);

% NaN exclusion
[T4, c4, p4] = crosstab([1 1 NaN 2], [1 2 1 2]);
fprintf('\n  NaN excluded: T size=[%d %d], chi2=%.4f\n', size(T4,1), size(T4,2), c4);
