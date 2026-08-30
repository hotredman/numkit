clear

x = [1.2 2.4 3.1 4.5 5.0]';
y = [0.8 1.9 2.7 4.0 4.5]';

fprintf('=== ttest paired form ===\n');
[h, p, ci, t] = ttest(x, y);
fprintf('  h=%d p=%.6f t=%.4f ci=[%.4f %.4f]\n', h, p, t, ci(1), ci(2));

fprintf('\n=== ttest Name-Value Alpha ===\n');
[~,~,ci95] = ttest(x, 4, 'Alpha', 0.05);
[~,~,ci99] = ttest(x, 4, 'Alpha', 0.01);
fprintf('  CI95 width = %.4f  CI99 width = %.4f (expect 99 wider)\n', ...
    ci95(2)-ci95(1), ci99(2)-ci99(1));

fprintf('\n=== ttest2 default (now equal/pooled, was unequal) ===\n');
[h, p, ci, t] = ttest2(x, y);
fprintf('  default t=%.6f (expect 0.475466)\n', t);
[h, p, ci, t] = ttest2(x, y, 'Vartype', 'unequal');
fprintf('  unequal t=%.6f\n', t);
