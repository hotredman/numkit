clear
import compat.*

% smoothdata 'sgolay' -- bugs/stats/smoothdata-methods.
% Degree-2 Savitzky-Golay smoothing. Matches MATLAB exactly for an explicit odd
% window (the auto default window is a data-dependent heuristic and only
% approximated). 'lowess'/'loess' are not yet implemented.

x = [1 5 2 8 3 9 4 7 2 6 1 8];

z5 = smoothdata(x, 'sgolay', 5);
fprintf('sgolay w5 = %s\n', num2str(z5, '%.5g '));
fprintf('  expect  : 1.1143 3.7429 5.0857 4.4 6.7714 5.4857 7 4.1714 5 2.6571 3.8286 7.1429\n');

z7 = smoothdata(x, 'sgolay', 7);
fprintf('sgolay w7 = %s\n', num2str(z7, '%.5g '));
fprintf('  expect  : 1.3333 3.2857 4.7143 5.619 5.5714 6.7619 5.2857 5.8095 3.4762 3.5714 4.5714 6.4762\n');

% lowess (local linear) + loess (local quadratic), tricube weights.
yl = smoothdata(x, 'lowess', 5);
fprintf('lowess w5 = %s\n', num2str(yl, '%.5g '));
fprintf('  expect  : 1.7113 3.0349 4.5768 4.8506 6.1494 5.8506 6.2905 4.7095 4.5768 3.4232 4.7908 6.1942\n');
yo = smoothdata(x, 'loess', 5);
fprintf('loess  w5 = %s\n', num2str(yo, '%.5g '));
fprintf('  expect  : 1.5509 2.9192 2 8 3 9 4 7 2 6 3.731 7.277  (interior unchanged: 3-pt quad)\n');
