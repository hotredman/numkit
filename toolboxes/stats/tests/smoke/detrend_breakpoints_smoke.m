clear

import compat.*

% detrend breakpoints (2026-05-30): MATLAB detrend(x, 1, bp) does a
% CONTINUOUS piecewise-linear detrend — the trend changes slope at each
% breakpoint but stays continuous. numkit previously IGNORED the 3rd
% (breakpoint) argument and returned the plain single-line linear detrend.
% vs MATLAB R2025b. (order-0 + breakpoints is a rare/ill-defined MATLAB
% edge and remains deferred.)

x = [1.1 1.8 3.3 3.9 5.2 5.7];

fprintf('=== single breakpoint at sample 3 ===\n');
fprintf('detrend(x,1,3)     = %s\n', mat2str(detrend(x, 1, 3), 6));
fprintf('  expect            [0.131579 -0.263158 0.142105 -0.147368 0.263158 -0.126316]\n');

fprintf('\n=== plain linear detrend (no breakpoint) differs ===\n');
fprintf('detrend(x,1)       = %s\n', mat2str(detrend(x, 1), 6));

fprintf('\n=== two breakpoints ===\n');
fprintf('detrend(x,1,[2 4]) = %s\n', mat2str(detrend(x, 1, [2 4]), 6));
fprintf('  expect            [~0 -0.131429 0.262857 -0.242857 0.222857 -0.111429]\n');

fprintf('\n=== matrix: each column detrended independently ===\n');
M = [x(:) x(:)+0.5];
disp(detrend(M, 1, 3));
fprintf('expect both columns = [0.131579; -0.263158; 0.142105; -0.147368; 0.263158; -0.126316]\n');
