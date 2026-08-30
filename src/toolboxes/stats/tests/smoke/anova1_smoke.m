clear

% anova1 — one-way ANOVA. Matrix form: each COLUMN of X is a group.
p = anova1([1 2 3; 2 3 4; 3 4 5]);
fprintf('anova1 matrix p = %.4f  (expect 0.1250)\n', p);

[p2, tbl] = anova1([1 2 3; 2 3 4; 3 4 5]);
fprintf('  SS between = %g, SS within = %g, F = %g  (expect 6, 6, 3)\n', ...
        tbl{2,2}, tbl{3,2}, tbl{2,5});

% Second matrix: 3 groups of 4.
fprintf('anova1 matrix2 p = %.4f  (expect 0.7000)\n', ...
        anova1([1 5 2; 7 3 8; 4 9 6; 2 1 5]));

% The (y, group) form is unchanged.
p3 = anova1([1 2 3 2 3 4 3 4 5], [1 1 1 2 2 2 3 3 3]);
fprintf('anova1 (y,group) p = %.4f  (expect 0.1250)\n', p3);
