clear

fprintf('=== exppdf ===\n');
fprintf('  default mu=1, x=2 : %.6f (expect 0.135335)\n', exppdf(2));
fprintf('  exppdf(2, 3)      : %.6f (expect 0.171139)\n', exppdf(2, 3));
y = exppdf([0 1 2 5], 2);
fprintf('  vec x, mu=2       : [%.4f %.4f %.4f %.4f]\n', y(1), y(2), y(3), y(4));
fprintf('  edges             : x<0 → %g (0), mu=0 → %g, mu<0 → %g (NaN)\n', exppdf(-1,2), exppdf(2,0), exppdf(2,-1));
