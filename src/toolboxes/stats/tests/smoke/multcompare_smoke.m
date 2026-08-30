clear

rng(0);

fprintf('=== multcompare — post-hoc pairwise comparisons ===\n');

% Three groups: 1 and 3 same mean, 2 shifted.
g1 = 10 + 0.5 * randn(20, 1);
g2 = 12 + 0.5 * randn(20, 1);
g3 = 10 + 0.5 * randn(20, 1);
y = [g1; g2; g3];
group = [ones(20, 1); 2 * ones(20, 1); 3 * ones(20, 1)];

[p, ~, stats] = anova1(y, group);
fprintf('\nANOVA: p = %g  (overall test of group equality)\n', p);

fprintf('\nstats struct fields populated:\n');
fprintf('  means = '); disp(stats.means');
fprintf('  n     = '); disp(stats.n');
fprintf('  s     = %.4f  (pooled std)\n', stats.s);
fprintf('  df    = %d   (error DOF)\n', stats.df);

fprintf('\nmultcompare (Bonferroni, default):\n');
fprintf('   i  j     lower    diff    upper    p-value\n');
c = multcompare(stats);
disp(c);
fprintf('  → (1,2) and (2,3) significantly different; (1,3) not.\n');

fprintf('\nmultcompare (LSD — no multiplicity correction):\n');
c_lsd = multcompare(stats, 0.05, 'lsd');
disp(c_lsd);
