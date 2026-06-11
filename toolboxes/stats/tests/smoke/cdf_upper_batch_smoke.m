clear

import compat.*

% Batch closure: every cdf_reg adapter now strips the trailing 'upper'
% string flag (case-insensitive) and returns 1 - F(x).  Probed below
% across the nine spec-touched distributions; expected lines hand-computed.

fprintf('=== evcdf — extreme value (Gumbel-min) ===\n');
fprintf('  evcdf(1, 0, 1)          = %.6f  (expect 0.934012)\n', evcdf(1, 0, 1));
fprintf('  evcdf(1, 0, 1, ''upper'') = %.6f  (expect 0.065988)\n\n', evcdf(1, 0, 1, 'upper'));

fprintf('=== geocdf — geometric ===\n');
fprintf('  geocdf(2, 0.3)          = %.6f  (expect 0.657)\n', geocdf(2, 0.3));
fprintf('  geocdf(2, 0.3, ''upper'') = %.6f  (expect 0.343)\n\n', geocdf(2, 0.3, 'upper'));

fprintf('=== gevcdf — generalised extreme value ===\n');
fprintf('  gevcdf(1, 0.5, 1, 0)          = %.6f\n', gevcdf(1, 0.5, 1, 0));
fprintf('  gevcdf(1, 0.5, 1, 0, ''upper'') = %.6f\n', gevcdf(1, 0.5, 1, 0, 'upper'));
fprintf('  sum should be 1.000000\n\n');

fprintf('=== gpcdf — generalised Pareto ===\n');
fprintf('  gpcdf(0.5, 0.5, 1, 0)          = %.6f\n', gpcdf(0.5, 0.5, 1, 0));
fprintf('  gpcdf(0.5, 0.5, 1, 0, ''upper'') = %.6f\n\n', gpcdf(0.5, 0.5, 1, 0, 'upper'));

fprintf('=== hygecdf — hypergeometric ===\n');
fprintf('  hygecdf(2, 50, 10, 8)          = %.6f\n', hygecdf(2, 50, 10, 8));
fprintf('  hygecdf(2, 50, 10, 8, ''upper'') = %.6f\n\n', hygecdf(2, 50, 10, 8, 'upper'));

fprintf('=== nakacdf — Nakagami-m ===\n');
fprintf('  nakacdf(1, 1, 1)          = %.6f  (expect 0.632121)\n', nakacdf(1, 1, 1));
fprintf('  nakacdf(1, 1, 1, ''upper'') = %.6f  (expect 0.367879)\n\n', nakacdf(1, 1, 1, 'upper'));

fprintf('=== nbincdf — negative binomial ===\n');
fprintf('  nbincdf(2, 3, 0.4)          = %.6f\n', nbincdf(2, 3, 0.4));
fprintf('  nbincdf(2, 3, 0.4, ''upper'') = %.6f\n\n', nbincdf(2, 3, 0.4, 'upper'));

fprintf('=== ncx2cdf — non-central chi-squared ===\n');
fprintf('  ncx2cdf(2, 3, 1)          = %.6f  (expect ~0.3083)\n', ncx2cdf(2, 3, 1));
fprintf('  ncx2cdf(2, 3, 1, ''upper'') = %.6f  (expect ~0.6917 per probe)\n\n', ncx2cdf(2, 3, 1, 'upper'));

fprintf('=== ricecdf — Rice (Rician) ===\n');
fprintf('  ricecdf(1, 1, 1)          = %.6f\n', ricecdf(1, 1, 1));
fprintf('  ricecdf(1, 1, 1, ''upper'') = %.6f\n', ricecdf(1, 1, 1, 'upper'));
fprintf('  sum should be 1.000000\n');
