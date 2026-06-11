clear
import compat.*

% filloutliers — detect + replace outliers.
% Reference: MATLAB R2025b.

x = [1 2 3 4 100 5 6 7];

fprintf('== fill methods (default median+3MAD detection) ==\n');
B = filloutliers(x, 'linear');
fprintf('  linear: %s (e [1 2 3 4 4.5 5 6 7])\n', mat2str(B));
B = filloutliers(x, 'previous');
fprintf('  prev: %s\n', mat2str(B));
B = filloutliers(x, 'next');
fprintf('  next: %s\n', mat2str(B));
B = filloutliers(x, 'nearest');
fprintf('  nearest (tie→next): %s\n', mat2str(B));
B = filloutliers(x, 'center');
fprintf('  center: %s\n', mat2str(B));
B = filloutliers(x, 'clip');
fprintf('  clip: %s (e [... 13.396 ...])\n', mat2str(B));
B = filloutliers(x, 0);
fprintf('  constant 0: %s\n', mat2str(B));

fprintf('\n== detection methods ==\n');
B = filloutliers(x, 'linear', 'mean');
fprintf('  mean (tf=3): %s (e unchanged, 100 not flagged)\n', mat2str(B));
B = filloutliers(x, 'linear', 'mean', 'ThresholdFactor', 1);
fprintf('  mean tf=1: %s\n', mat2str(B));
B = filloutliers(x, 'linear', 'quartiles');
fprintf('  quartiles: %s\n', mat2str(B));
