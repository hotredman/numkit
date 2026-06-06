% Multi-file example — main.m calls helper.m and format_result.m
% from the same folder, no addpath needed.
%
% MATLAB convention: every .m file in the script's containing
% directory is automatically callable by short name. Open this
% file in the IDE and click Run — the engine resolves
% `compute_stats(...)` and `format_result(...)` against the sibling
% files `compute_stats.m` and `format_result.m`.
%
% Pull toolbox functions into scope so we can use mean / std / etc
% without fully-qualified names.
import compat.*;

clear all

% Some toy data
data = [42, 17, 99, 8, 56, 23, 71, 34, 11, 88];

fprintf('Input data:\n');
disp(data)

% Call sibling: compute_stats.m
stats = compute_stats(data);

% Call another sibling: format_result.m
fprintf('\n');
format_result('Mean',   stats.mean);
format_result('Median', stats.median);
format_result('Std',    stats.std);
format_result('Min',    stats.min);
format_result('Max',    stats.max);
format_result('Range',  stats.range);

fprintf('\nDone — both helpers were resolved from the same folder.\n');
