clear

% Friedman nonparametric two-way ANOVA by ranks -- bugs/stats/friedman.
% Rank the k treatments (columns) within each of the n blocks (rows); the
% tie-corrected chi-square statistic Q gives p = 1 - chi2cdf(Q, k-1).
% numkit returns [p, Q, df] (statistic + df), not MATLAB's display (tbl, stats);
% the primary p-value matches MATLAB exactly.

[p, Q, df] = friedman([1 2 3; 2 3 4; 3 4 5; 1 3 5], 1);
fprintf('no ties : p=%.6f  Q=%.4f  df=%g   (expect 0.018316  8.0000  2)\n', p, Q, df);

p2 = friedman([7 9 8; 6 5 7; 9 7 6; 8 8 9; 5 6 5], 1);
fprintf('with ties: p=%.6f   (expect 0.846482)\n', p2);
