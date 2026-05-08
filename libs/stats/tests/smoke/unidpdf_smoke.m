clear

import compat.*

fprintf('=== unidpdf ===\n');
fprintf('  unidpdf(3, 6)   : %.6f (expect 0.166667 = 1/6)\n', unidpdf(3, 6));
y = unidpdf([1 2 3 4 5 6 7], 6);
fprintf('  vec k=1..7,N=6  : '); fprintf('%.4f ', y); fprintf('\n');
fprintf('  out-of-support  : k=0 → %g, k=7 → %g, k=2.5 → %g (expect 0)\n', ...
    unidpdf(0, 6), unidpdf(7, 6), unidpdf(2.5, 6));
fprintf('  NaN k           : %g (expect 0)\n', unidpdf(NaN, 6));
fprintf('  bad N           : N=0 → %g, N=-1 → %g, N=6.5 → %g, N=NaN → %g (NaN)\n', ...
    unidpdf(3, 0), unidpdf(3, -1), unidpdf(3, 6.5), unidpdf(3, NaN));
