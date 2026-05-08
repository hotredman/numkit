clear

import compat.*

fprintf('=== binopdf ===\n');
fprintf('  Bin(10, 0.3) at 3 = %.6f (expect 0.266828)\n', binopdf(3, 10, 0.3));
y = binopdf([0 3 5 10], 10, 0.3);
fprintf('  vector k          : [%.4f %.4f %.4f %.6f]\n', y(1), y(2), y(3), y(4));
fprintf('  out-of-support    : x<0 → %g, x>n → %g, non-int → %g (all 0)\n', binopdf(-1,10,0.3), binopdf(11,10,0.3), binopdf(3.5,10,0.3));
fprintf('  p=0, k=0          : %g (expect 1)\n', binopdf(0, 10, 0));
fprintf('  p=1, k=n          : %g (expect 1)\n', binopdf(10, 10, 1));
fprintf('  invalid           : n<0 → %g, p<0 → %g (NaN)\n', binopdf(3,-1,0.3), binopdf(3,10,-0.1));
