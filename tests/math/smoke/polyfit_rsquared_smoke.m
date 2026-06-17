clear
import compat.*

% The 2nd output of polyfit, the error-estimate struct S, now exposes the
% rsquared field (the coefficient of determination R^2 = 1 - SSresid/SStot
% = 1 - (normr/norm(y-mean(y)))^2), matching MATLAB R2025b. numkit
% previously returned only {R, df, normr} -- the 4th field rsquared was
% missing.

x = (1:6)';
y = [1.1 4.2 9.1 15.8 25.3 35.9]';
[p, S, mu] = polyfit(x, y, 2);
fn = fieldnames(S);
fprintf('--- polyfit(x, y, 2) ---\n');
fprintf('S fields (%d):', numel(fn));
for i = 1:numel(fn), fprintf(' %s', fn{i}); end
fprintf('   (expect 4: R df normr rsquared)\n');
fprintf('S.normr=%.8f  S.rsquared=%.8f   (expect 0.39865846, 0.99982100)\n', ...
        S.normr, S.rsquared);

% An exact fit gives rsquared = 1.
[~, S2] = polyfit((1:5)', (2*(1:5) + 1)', 1);
fprintf('--- exact linear fit ---\n');
fprintf('S.rsquared=%.8f   (expect 1)\n', S2.rsquared);

% A poor fit (constant model through scattered data) gives a low R^2.
[~, S3] = polyfit((1:5)', [1 5 2 8 3]', 0);
fprintf('--- degree-0 fit of scattered data ---\n');
fprintf('S.rsquared=%.8f   (expect ~0)\n', S3.rsquared);
