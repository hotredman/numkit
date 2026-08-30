clear

% isoutlier now supports the 'grubbs' method: an ITERATIVE Grubbs's test. The
% ThresholdFactor for 'grubbs' is the SIGNIFICANCE LEVEL alpha (default 0.05),
% not a multiplier. At each step the point with the largest studentized
% deviation G = max|x-mean|/std (std with N-1) is compared to the Grubbs
% critical value G_crit = ((N-1)/sqrt(N))*sqrt(t^2/(N-2+t^2)) with
% t = tinv(alpha/(2N), N-2); if G > G_crit that point is flagged and removed,
% then the test repeats (down to N>=3). numkit previously threw
% "method 'grubbs' is not supported".

fprintf('--- a lone large outlier ---\n');
g = isoutlier([1 2 3 4 5 6 7 8 9 50], 'grubbs');
fprintf('mask:'); fprintf(' %d', g); fprintf('   (expect 0..0 1)  sum=%d\n', sum(g));

fprintf('--- two opposite extremes mask each other ---\n');
g2 = isoutlier([10 12 11 13 12 11 50 12 11 -30 12], 'grubbs');
fprintf('sum=%d   (expect 0 -- Grubbs masking effect)\n', sum(g2));

fprintf('--- stricter significance alpha=0.01 ---\n');
ga = isoutlier([1 2 3 4 5 6 7 8 9 50], 'grubbs', 'ThresholdFactor', 0.01);
fprintf('sum=%d   (expect 1)\n', sum(ga));

fprintf('--- per-column on a matrix ---\n');
gm = isoutlier([1 2; 2 3; 3 4; 4 5; 100 6; 5 50], 'grubbs');
fprintf('col1 sum=%d  col2 sum=%d   (expect 1 1)\n', sum(gm(:,1)), sum(gm(:,2)));

fprintf('--- other methods unaffected ---\n');
fprintf('median=%d mean=%d quartiles=%d   (expect 1 0 1)\n', ...
        sum(isoutlier([1 2 3 4 100], 'median')), ...
        sum(isoutlier([1 2 3 4 100], 'mean')), ...
        sum(isoutlier([1 2 3 4 100], 'quartiles')));
