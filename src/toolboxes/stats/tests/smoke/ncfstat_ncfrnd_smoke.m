clear

[m, v] = ncfstat(5, 10, 3);
fprintf('ncfstat(5, 10, 3) : m=%.10f v=%.10f  (expect 2.0, 3.1666666667)\n', m, v);

[m, v] = ncfstat(8, 20, 2);
fprintf('ncfstat(8, 20, 2) : m=%.10f v=%.10f  (expect 1.3888888889, 0.7619598765)\n', m, v);

rng(0);
S = ncfrnd(5, 10, 3, 5000, 1);
fprintf('\nncfrnd(5, 10, 3, 5000, 1):\n');
fprintf('  sample mean = %.4f  (true 2.0)\n', mean(S));
fprintf('  sample var  = %.4f  (true 3.17)\n', var(S));

% Central limit
rng(0);
S0 = ncfrnd(5, 10, 0, 5000, 1);
fprintf('\nDelta=0 limit (central F): sample mean = %.4f  (true 1.25)\n', mean(S0));
