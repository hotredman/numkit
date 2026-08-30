clear

fprintf('=== lognpdf ===\n');
fprintf('  default LN(0,1) at 2 : %.6f (expect 0.156874)\n', lognpdf(2));
y = lognpdf([0 1 2 5], 0, 1);
fprintf('  vector x             : [%.4f %.4f %.4f %.4f]\n', y(1), y(2), y(3), y(4));
fprintf('  edges                : x<0 → %g, x=0 → %g (both 0), sigma=0 → %g (NaN)\n', lognpdf(-1,0,1), lognpdf(0,0,1), lognpdf(2,0,0));
