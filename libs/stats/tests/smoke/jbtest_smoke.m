clear;
import compat.*;

% jbtest — Jarque-Bera normality test. Closes
% audit/findings/stats/jbtest.md.
%
% New 2026-05-08:
%   - Small-sample (n < 2000) p-value via Monte-Carlo simulation
%     under H₀ (matches MATLAB R2025b's tabulated-p behavior).
%   - p capped at 0.5 like MATLAB.
%   - mctol argument: when supplied, controls Monte-Carlo SE.
%   - Large n uses χ²(2) asymptotic (back-compat); pass mctol=NaN
%     to force asymptotic at any n.
%   - Deterministic seed for MC → reproducible cv.

xn = [-0.5 0.3 0.7 1.1 -0.2 0.1 -0.4 0.8 -0.1 0.5]';

fprintf('--- basic [h, p, JB, cv] = jbtest(xn) (n=10, MC) ---\n');
[h, p, JB, cv] = jbtest(xn);
fprintf('h=%d p=%.4f JB=%.4f cv=%.4f\n', h, p, JB, cv);
fprintf('expect h=0 p=0.5 JB=0.6648 cv=2.5239\n\n');

fprintf('--- alpha=0.01 ---\n');
[h, p, JB, cv] = jbtest(xn, 0.01);
fprintf('h=%d p=%.4f JB=%.4f cv=%.4f\n', h, p, JB, cv);
fprintf('expect p=0.5 cv=5.7077\n\n');

fprintf('--- with mctol=0.01 (faster MC, noisier cv) ---\n');
[h, p, JB, cv] = jbtest(xn, 0.05, 0.01);
fprintf('h=%d p=%.4f JB=%.4f cv=%.4f\n', h, p, JB, cv);
fprintf('expect p~0.5 cv~2.5\n\n');

fprintf('--- mctol=NaN forces asymptotic χ²(2) at small n ---\n');
[h, p, JB, cv] = jbtest(xn, 0.05, NaN);
fprintf('h=%d p=%.4f JB=%.4f cv=%.4f\n', h, p, JB, cv);
fprintf('expect cv=5.9915 (chi2inv(0.95, 2))\n\n');

fprintf('--- large n=3000 uses asymptotic ---\n');
xb = ones(3000, 1);
[h, p, JB, cv] = jbtest(xb);
fprintf('h=%d p=%.4f JB=%.4f cv=%.4f\n', h, p, JB, cv);
fprintf('expect cv=5.9915 (asymptotic at n>=2000)\n');
