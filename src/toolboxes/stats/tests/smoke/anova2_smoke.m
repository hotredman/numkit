clear
import compat.*

fprintf('=== anova2 (two-way ANOVA without replication) ===\n');

% Classic textbook example: 4 fertilizers x 3 fields
Y = [55 26 78;
     60 24 73;
     70 28 75;
     65 27 80];
[p, tbl] = anova2(Y);
fprintf('  4 fertilizers x 3 fields:\n');
fprintf('    p_cols (fields differ?) = %g (expect ~5e-6 -- strong effect)\n', p(1));
fprintf('    p_rows (fertilizers differ?) = %g (expect ~0.30 -- no effect)\n', p(2));

fprintf('\n=== Strong row effect, identical columns ===\n');
Yr = [1 1 1; 5 5 5; 10 10 10];
pr = anova2(Yr);
fprintf('    p_cols = %g (expect ~1)\n', pr(1));
fprintf('    p_rows = %g (expect 0)\n', pr(2));

fprintf('\n=== ANOVA table (5x6 cell) for fertilizer example ===\n');
disp(tbl);
