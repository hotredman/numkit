clear

% corr 'Type' Spearman/Kendall (was silently ignored -> always Pearson).
x = [1;2;3;4]; y = [1;4;9;16];   % monotonic but nonlinear
fprintf('Pearson  = %.6f (expect 0.984374)\n', corr(x, y));
fprintf('Spearman = %.6f (expect 1.000000)\n', corr(x, y, 'type', 'Spearman'));
fprintf('Kendall  = %.6f (expect 1.000000)\n', corr(x, y, 'type', 'Kendall'));

% Kendall tau-b vs Spearman with ties.
c = [1;1;2;3;3]; d = [1;2;2;3;4];
fprintf('Spearman(ties) = %.6f (expect 0.892218)\n', corr(c, d, 'type', 'Spearman'));
fprintf('Kendall(ties)  = %.6f (expect 0.824958)\n', corr(c, d, 'type', 'Kendall'));

% Non-monotonic: Kendall (0.6) differs from Spearman (0.8).
a = [1;2;3;4;5]; b = [2;1;4;3;5];
fprintf('Spearman(ab) = %.6f (expect 0.8)\n', corr(a, b, 'type', 'Spearman'));
fprintf('Kendall(ab)  = %.6f (expect 0.6)\n', corr(a, b, 'type', 'Kendall'));

% Matrix form: column-pair correlation matrix.
M = [1 2; 2 1; 3 4; 4 3; 5 5];
R = corr(M, 'type', 'Spearman');
fprintf('corr(M,Spearman) R(1,2) = %.6f (expect 0.8)\n', R(1,2));
