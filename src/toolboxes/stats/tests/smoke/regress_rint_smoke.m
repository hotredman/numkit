clear

% regress's 4th output rint gives a confidence interval for each residual,
% used to diagnose outliers: if rint(i,:) does NOT contain 0, observation i
% is a likely outlier. MATLAB R2025b computes these via the Chatterjee & Hadi
% LEAVE-ONE-OUT studentized-residual variance estimate with (nu-1) degrees of
% freedom -- NOT the textbook r +/- t*sigma*sqrt(1-h). numkit used the
% textbook form (rint disagreed with MATLAB); now it matches.

y = [1.1 1.9 3.2 3.8 5.1]';
X = [ones(5,1) (1:5)'];
[b, bint, r, rint, stats] = regress(y, X);

fprintf('--- regress(y, X): residuals + intervals ---\n');
for i = 1:5
    fprintf('r(%d)=% .4f  rint(%d,:)=[% .8f % .8f]\n', i, r(i), i, rint(i,1), rint(i,2));
end
fprintf('expect rint(1,:)=[-0.54237138  0.66237138]\n');
fprintf('       rint(3,:)=[-0.52174142  0.88174142]\n');
fprintf('       rint(5,1)=-0.45100840\n');

% No interval excludes 0 -> no outliers flagged (this is clean-ish data).
nOut = sum(rint(:,1) > 0 | rint(:,2) < 0);
fprintf('outliers flagged: %d (expect 0)\n', nOut);
