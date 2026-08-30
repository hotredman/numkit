clear

% bugs/stats/corr-pvalue.md — [r, p] = corr(...) 2nd output (p-value).
% Reference values from MATLAB R2025b.

x = [1 2 3 4 5]';
y = [2 1 4 3 6]';

% --- Pearson (default): t-distribution p-value -------------------------
[r, p] = corr(x, y);
fprintf('Pearson : r = %.6f  p = %.9f   (expect r~0.821995  p~0.087706647)\n', r, p);

% --- Spearman: exact permutation p (small n) ---------------------------
[rs, ps] = corr(x, y, 'type', 'Spearman');
fprintf('Spearman: r = %.6f  p = %.9f   (expect r~0.8       p~0.133333333)\n', rs, ps);

[~, ps7] = corr([1 2 3 4 5 6 7]', [2 1 4 3 6 5 7]', 'type', 'Spearman');
fprintf('Spearman n7         p = %.9f   (expect            p~0.012301587)\n', ps7);

% --- Kendall: exact inversions-distribution p --------------------------
[rk, pk] = corr(x, y, 'type', 'Kendall');
fprintf('Kendall : r = %.6f  p = %.9f   (expect r~0.6       p~0.233333333)\n', rk, pk);

[~, pk8] = corr([1 2 3 4 5 6 7 8]', [2 1 4 3 6 5 8 7]', 'type', 'Kendall');
fprintf('Kendall n8          p = %.9f   (expect            p~0.014136905)\n', pk8);

% --- Matrix form corr(X): p has r's shape, diagonal = 1 ----------------
X = [1 2 3 4 5; 2 1 4 3 6; 1 3 2 5 4]';
[R, P] = corr(X);
fprintf('Matrix diag P       = [%.0f %.0f %.0f]      (expect [1 1 1])\n', P(1,1), P(2,2), P(3,3));
fprintf('Matrix P(1,2)       = %.9f   (expect            p~0.087706647)\n', P(1,2));
