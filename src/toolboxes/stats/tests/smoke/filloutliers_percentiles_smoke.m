clear

% filloutliers(A, fillmethod, 'percentiles', [lo hi]) — DEEP-PROBE
% 2026-05-31. The 'percentiles' detection method was unsupported
% (filloutliers threw "findmethod must be 'median', 'mean', or
% 'quartiles'"). Percentile bounds use MATLAB's prctile convention
% (100*(k-0.5)/n); the 'center' fill uses the midpoint (lo+hi)/2 and
% 'clip' clips outliers to [lo, hi]. Reference: MATLAB R2025b.

x = [1 2 3 100 4 5];

fprintf('=== clip, percentiles [10 90] ===\n');
cp = filloutliers(x, 'clip', 'percentiles', [10 90]);
fprintf('%g %g %g %g %g %g  (expect 1.1 2 3 90.5 4 5)\n', ...
        cp(1),cp(2),cp(3),cp(4),cp(5),cp(6));

fprintf('\n=== center, percentiles [10 90] (midpoint 45.8) ===\n');
ce = filloutliers(x, 'center', 'percentiles', [10 90]);
fprintf('%g %g %g %g %g %g  (expect 45.8 2 3 45.8 4 5)\n', ...
        ce(1),ce(2),ce(3),ce(4),ce(5),ce(6));

fprintf('\n=== constant 0, percentiles [10 90] ===\n');
c0 = filloutliers(x, 0, 'percentiles', [10 90]);
fprintf('%g %g %g %g %g %g  (expect 0 2 3 0 4 5)\n', ...
        c0(1),c0(2),c0(3),c0(4),c0(5),c0(6));

fprintf('\n=== clip, percentiles [25 75] ===\n');
c = filloutliers(x, 'clip', 'percentiles', [25 75]);
fprintf('%g %g %g %g %g %g  (expect 2 2 3 5 4 5)\n', ...
        c(1),c(2),c(3),c(4),c(5),c(6));

fprintf('\n=== matrix clip, percentiles [20 80] (per column) ===\n');
R = filloutliers([1 2; 3 4; 5 100; 7 8; 9 10], 'clip', 'percentiles', [20 80]);
fprintf('col1 = %g %g %g %g %g  (expect 2 3 5 7 8)\n', R(1,1),R(2,1),R(3,1),R(4,1),R(5,1));
fprintf('col2 = %g %g %g %g %g  (expect 3 4 55 8 10)\n', R(1,2),R(2,2),R(3,2),R(4,2),R(5,2));
