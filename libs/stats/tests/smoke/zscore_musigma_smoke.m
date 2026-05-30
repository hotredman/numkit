clear

import compat.*

% zscore second/third outputs [Z, MU, SIGMA]. Bug fixed 2026-05-30: zscore
% returned only Z; requesting MU/SIGMA errored. SIGMA is the N-1 sample std
% by default (population std for flag==1). vs MATLAB R2025b.

format long

fprintf('=== vector ===\n');
[z, mu, sg] = zscore([2 4 6 8]);
fprintf('z = %s\n', mat2str(z, 6));
fprintf('mu = %g  sigma = %g (expect 5 / 2.58199)\n', mu, sg);

fprintf('\n=== matrix, dim 1 (column stats as row vectors) ===\n');
[zm, mum, sgm] = zscore([1 2; 3 6; 5 10]);
fprintf('mu = %s  sigma = %s (expect [3 6] / [2 4])\n', mat2str(mum), mat2str(sgm));

fprintf('\n=== population flag (sigma uses N) ===\n');
[zp, mup, sgp] = zscore([2 4 6 8], 1);
fprintf('sigma_pop = %g (expect 2.23607 = sqrt(5))\n', sgp);

fprintf('\n=== single output still works ===\n');
fprintf('z = %s\n', mat2str(zscore([2 4 6 8]), 6));
