clear
import compat.*

% histcounts 'Normalization' modes. x has 2 out-of-range points, so the
% normalization total N = numel(x) = 9 (NOT the in-range count of 7).
x = [1 2 2 3 3 3 5 99 -7];
e = [0 2 4 6];

fprintf('count       : %s\n', mat2str(histcounts(x, e)));                              % [1 5 1]
fprintf('probability : %s\n', mat2str(histcounts(x, e, 'Normalization','probability'))); % [1 5 1]/9
fprintf('cumcount    : %s\n', mat2str(histcounts(x, e, 'Normalization','cumcount')));    % [1 6 7]
fprintf('cdf         : %s\n', mat2str(histcounts(x, e, 'Normalization','cdf')));          % [1 6 7]/9

% Nonuniform edges → per-bin binwidths [1 3 2]; all 4 data points in range.
xu = [0.5 2 3 5];
eu = [0 1 4 6];
fprintf('countdensity: %s\n', mat2str(histcounts(xu, eu, 'Normalization','countdensity'))); % [1 2/3 1/2]
fprintf('pdf         : %s\n', mat2str(histcounts(xu, eu, 'Normalization','pdf')));           % [1/4 1/6 1/8]
