clear

import compat.*

fprintf('=== wblpdf ===\n');
fprintf('  default a=b=1 (=exponential):\n');
fprintf('    at 1   : %.6f (expect 0.367879 = e^-1)\n', wblpdf(1));
fprintf('    at 0.5 : %.6f (expect 0.606531)\n', wblpdf(0.5));
fprintf('    at 0   : %g (expect 1 = b/a)\n', wblpdf(0));
fprintf('    at -1  : %g (expect 0)\n', wblpdf(-0.5));
fprintf('  scaled (a=2, b=3):\n');
fprintf('    at 1   : %.6f (expect 0.330936)\n', wblpdf(1, 2, 3));
fprintf('  density at x=0 by shape:\n');
fprintf('    b=1 → %g (1), b=0.5 → %g (Inf), b=2 → %g (0)\n', ...
    wblpdf(0, 1, 1), wblpdf(0, 1, 0.5), wblpdf(0, 1, 2));
y = wblpdf([0 0.5 1 2 5], 1, 2);
fprintf('  vec (a=1,b=2)  : [%g %.4f %.4f %.4f %g]\n', y(1), y(2), y(3), y(4), y(5));
fprintf('  bad params: a<=0 → %g, b<=0 → %g, a=NaN → %g (NaN)\n', ...
    wblpdf(1, 0, 1), wblpdf(1, 1, 0), wblpdf(1, NaN, 1));
fprintf('  NaN x → %g (NaN)\n', wblpdf(NaN, 1, 1));
