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

% 4th output: labels (cell of num2str value strings, padded with [])
fprintf('\n=== 4th output: labels ===\n');
[T5, ~, ~, lab] = crosstab([1 1 2 2 3 3], [10 20 10 20 10 20]);
fprintf('  size(lab) = [%d %d], iscell = %d   (expect [3 2], 1)\n', ...
        size(lab,1), size(lab,2), iscell(lab));
fprintf('  col 1 (unique x): %s %s %s   (expect 1 2 3)\n', lab{1,1}, lab{2,1}, lab{3,1});
fprintf('  col 2 (unique y): %s %s   (expect 10 20)\n', lab{1,2}, lab{2,2});
fprintf('  lab{3,2} padded with []: isempty = %d   (expect 1)\n', isempty(lab{3,2}));

% Unequal category counts: x=2 lvls, y=4 lvls -> labels is 4x2
[~, ~, ~, lab2] = crosstab([1 1 1 1 2 2 2 2], [5 6 7 8 5 6 7 8]);
fprintf('  unequal counts: size(lab2)=[%d %d], lab2{4,2}=%s, isempty(lab2{3,1})=%d\n', ...
        size(lab2,1), size(lab2,2), lab2{4,2}, isempty(lab2{3,1}));

% Single-arg form: labels is R x 1
[~, ~, ~, labs] = crosstab([3 3 1 1 1 2]);
fprintf('  single-arg labels: size=[%d %d], %s %s %s   (expect [3 1], 1 2 3)\n', ...
        size(labs,1), size(labs,2), labs{1,1}, labs{2,1}, labs{3,1});
