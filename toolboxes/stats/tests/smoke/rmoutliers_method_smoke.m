clear

import compat.*

% rmoutliers(A, method[, percentiles][,'ThresholdFactor',tf]) — DEEP-PROBE
% 2026-05-31. The method/percentiles/ThresholdFactor args were parsed but
% IGNORED (always the default median/MAD detector), and matrices were
% flattened to a vector instead of having outlier ROWS removed.
% Reference: MATLAB R2025b.

x = [1 2 3 100 4 5];

fprintf('=== default (median) ===\n');
disp(rmoutliers(x));                              % expect [1 2 3 4 5]

fprintf('=== mean (100 within 3*std -> no removal) ===\n');
disp(rmoutliers(x, 'mean'));                      % expect [1 2 3 100 4 5]

fprintf('=== quartiles ===\n');
disp(rmoutliers(x, 'quartiles'));                 % expect [1 2 3 4 5]

fprintf('=== percentiles [10 90] ===\n');
disp(rmoutliers(x, 'percentiles', [10 90]));      % expect [2 3 4 5]

fprintf('=== median, ThresholdFactor 1 ===\n');
disp(rmoutliers(x, 'median', 'ThresholdFactor', 1)); % expect [2 3 4 5]

fprintf('=== matrix (remove outlier rows) ===\n');
M = [1 2; 3 4; 5 100; 7 8; 9 10];
disp(rmoutliers(M));                              % expect rows [1 2;3 4;7 8;9 10]

fprintf('=== 2nd output: removed mask ===\n');
[~, iv] = rmoutliers(x);
fprintf('vector mask = [%g %g %g %g %g %g]  (expect [0 0 0 1 0 0])\n', ...
        iv(1),iv(2),iv(3),iv(4),iv(5),iv(6));
[~, iM] = rmoutliers(M);
fprintf('matrix removed-rows (%dx%d) = [%g %g %g %g %g]  (expect 5x1 [0 0 1 0 0])\n', ...
        size(iM,1), size(iM,2), iM(1),iM(2),iM(3),iM(4),iM(5));
