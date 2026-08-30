clear

% fillmissing — 'nearest' and 'linear' methods (added cycle 74) plus
% per-column matrix processing.
% Reference: MATLAB R2025b.

fprintf('== nearest ==\n');
B = fillmissing([1 NaN NaN 4 NaN 6], 'nearest');
fprintf('  basic: %s (e [1 1 4 4 6 6])\n', mat2str(B));
B = fillmissing([NaN NaN 3 NaN 5 NaN NaN], 'nearest');
fprintf('  ends: %s (e [3 3 3 5 5 5 5])\n', mat2str(B));

fprintf('\n== linear ==\n');
B = fillmissing([1 NaN NaN 4 NaN 6], 'linear');
fprintf('  interior: %s (e [1 2 3 4 5 6])\n', mat2str(B));
B = fillmissing([NaN NaN 3 NaN 5 NaN NaN], 'linear');
fprintf('  extrap: %s (e [1 2 3 4 5 6 7])\n', mat2str(B));
B = fillmissing([NaN 1 2 NaN 4 NaN], 'linear');
fprintf('  L/T: %s (e [0 1 2 3 4 5])\n', mat2str(B));

fprintf('\n== matrix linear (per-column) ==\n');
B = fillmissing([1 NaN; NaN 4; 3 NaN; 5 8], 'linear');
fprintf('  col1: [%g %g %g %g]  (e 1 2 3 5)\n', B(1,1), B(2,1), B(3,1), B(4,1));
fprintf('  col2: [%g %g %g %g]  (e 2 4 6 8)\n', B(1,2), B(2,2), B(3,2), B(4,2));

fprintf('\n== matrix previous (per-column, was 1D bug) ==\n');
B = fillmissing([1 NaN; NaN 4; 3 NaN; 5 8], 'previous');
fprintf('  col1: [%g %g %g %g]  (e 1 1 3 5)\n', B(1,1), B(2,1), B(3,1), B(4,1));
fprintf('  col2: [%g %g %g %g]  (e NaN 4 4 8)\n', B(1,2), B(2,2), B(3,2), B(4,2));
