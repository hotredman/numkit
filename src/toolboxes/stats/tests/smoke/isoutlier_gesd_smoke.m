clear

% isoutlier(x,'gesd') — generalized ESD test. Fixed 2026-06-05
% (bugs/stats/isoutlier-gesd.md). Reference: MATLAB R2025b.

show = @(label, v) fprintf('%-28s %s\n', label, num2str(double(v)));

show('repro [..50]:',           isoutlier([1 2 3 4 5 6 7 8 9 50], 'gesd'));     % expect last=1
show('mid [1 2 3 100 4 5]:',    isoutlier([1 2 3 100 4 5], 'gesd'));            % expect 4th=1
show('clean 1:10:',             isoutlier([1 2 3 4 5 6 7 8 9 10], 'gesd'));     % expect all 0
show('spread n25 (peel 3):',    isoutlier([zeros(1,22) 20 30 40], 'gesd'));     % expect last 3 = 1
show('mask n15 default(->0):',  isoutlier([zeros(1,10) 100 101 102 103 104], 'gesd'));      % expect all 0
show('mask n15 MaxNumOut=5:',   isoutlier([zeros(1,10) 100 101 102 103 104], 'gesd', 'MaxNumOutliers', 5));  % expect 5
show('tf=0.01:',                isoutlier([1 2 3 4 5 6 7 8 9 50], 'gesd', 'ThresholdFactor', 0.01));  % expect last=1
show('small n5:',               isoutlier([0 0 0 0 50], 'gesd'));               % expect last=1
