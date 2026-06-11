clear
import compat.*

% ksdensity now supports 'Function','icdf' (the inverse of the smoothed CDF).
% When Function='icdf' the SECOND argument is a vector of PROBABILITIES in
% [0,1]; the result is the quantile x such that cdf(x) = p. It is computed by
% Newton's method on the kernel-smoothed cdf/pdf (seeded by linear inverse-
% interpolation of a 100-point grid cdf), matching MATLAB R2025b to ~1e-10.
% numkit previously threw "icdf is not yet supported".

x = [1 2 2.5 3 3.5 4 5 6 7 9]';

fprintf('--- icdf at standard quantile probabilities ---\n');
p = [0.1 0.25 0.5 0.75 0.9];
ic = ksdensity(x, p, 'Function', 'icdf');
fprintf('p   :'); fprintf(' %6.2f', p);  fprintf('\n');
fprintf('icdf:'); fprintf(' %6.3f', ic); fprintf('\n');
fprintf('(expect ~ 0.919 2.276 4.017 6.157 8.206)\n');

fprintf('--- round-trip icdf(cdf(x0)) == x0 ---\n');
c = ksdensity(x, 4.0, 'Function', 'cdf');
rt = ksdensity(x, c, 'Function', 'icdf');
fprintf('cdf(4.0)=%.6f  icdf(that)=%.6f   (expect 4.0)\n', c, rt);

fprintf('--- boundary / out-of-range probabilities ---\n');
e = ksdensity(x, [0 1 -0.1 1.1], 'Function', 'icdf');
fprintf('p=0 ->%g  p=1 ->%g  p=-0.1 ->%g  p=1.1 ->%g\n', e(1), e(2), e(3), e(4));
fprintf('(expect -Inf, Inf, NaN, NaN)\n');
