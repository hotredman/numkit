clear

import compat.*

% bugs/builtin/gamma-negative-integer-poles.md — gamma at non-positive integer
% poles returns +Inf (NOT NaN), matching MATLAB. Non-integer negatives and
% positive args are unchanged.

g = gamma([-1 -2 -3 0 0.5 -0.5 5]);
fprintf('gamma([-1 -2 -3 0 0.5 -0.5 5]) = ');
for i = 1:numel(g); fprintf('%.5g ', g(i)); end
fprintf('\n  expect Inf Inf Inf Inf 1.7725 -3.5449 24\n');

fprintf('gamma(-Inf) = %.5g   expect Inf\n', gamma(-Inf));
fprintf('gamma(Inf)  = %.5g   expect Inf\n', gamma(Inf));
fprintf('gamma(NaN)  = %.5g   expect NaN\n', gamma(NaN));
fprintf('isinf(gamma(-1))=%d (expect 1), gamma(-1)>0 -> %d (expect 1, +Inf)\n', ...
        isinf(gamma(-1)), gamma(-1) > 0);
