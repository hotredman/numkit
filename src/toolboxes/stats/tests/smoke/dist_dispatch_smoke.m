clear
import compat.*

% generic distribution dispatchers cdf/pdf/icdf/random -- bugs/stats/distribution-dispatchers.
% Map a distribution NAME (case-insensitive + aliases) to the per-family builtin
% and forward the params: cdf->'<fam>cdf', pdf->'<fam>pdf', icdf->'<fam>inv',
% random->'<fam>rnd'.

fprintf('cdf(Normal,1,0,1)      = %.6f   (expect 0.841345)\n', cdf('Normal',1,0,1));
fprintf('pdf(Poisson,2,3)       = %.6f   (expect 0.224042)\n', pdf('Poisson',2,3));
fprintf('icdf(Normal,0.975,0,1) = %.6f   (expect 1.959964)\n', icdf('Normal',0.975,0,1));
fprintf('cdf(Gamma,2,3,1)       = %.6f   (expect 0.323324)\n', cdf('Gamma',2,3,1));
fprintf('icdf(Chisquare,0.95,3) = %.6f   (expect 7.814728)\n', icdf('Chisquare',0.95,3));
fprintf('pdf(Binomial,2,5,0.3)  = %.6f   (expect 0.308700)\n', pdf('Binomial',2,5,0.3));
fprintf('cdf(norm,1,0,1) alias  = %.6f   (expect 0.841345)\n', cdf('norm',1,0,1));

r = random('Normal', 0, 1, 2, 3);
fprintf('random(Normal,0,1,2,3) size = %dx%d   (expect 2x3)\n', size(r,1), size(r,2));
