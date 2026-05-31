clear
import compat.*
% datestr of MULTIPLE dates -> N-row char matrix. DEEP-PROBE 2026-05-31:
% numkit threw "multi-date matrix input not yet supported" and the numel==6
% check mis-treated a 6x1 column as a single date vector. MATLAB rule:
% an N-by-6 matrix is N DATE VECTORS; anything else is serial NUMBERS
% (column-major, numel rows).

M = datestr([738885;738886;738900]);
fprintf('col vector: rows=%d (expect 3) cols=%d (expect 11)\n', size(M,1), size(M,2));
fprintf('  [%s] [%s] [%s]  (expect 30-Dec-2022 / 31-Dec-2022 / 14-Jan-2023)\n', M(1,:), M(2,:), M(3,:));

R = datestr([738885 738886]);
fprintf('row vector: rows=%d (expect 2) r2=[%s] (expect 31-Dec-2022)\n', size(R,1), R(2,:));

F = datestr([738885;738886], 'yyyy-mm-dd');
fprintf('with format: cols=%d (expect 10) r1=[%s] (expect 2022-12-30)\n', size(F,2), F(1,:));

DV = datestr([2020 1 1 0 0 0; 2021 6 15 12 30 0]);
fprintf('2x6 datevec: rows=%d (expect 2)\n', size(DV,1));
fprintf('  [%s] [%s]  (expect 01-Jan-2020 00:00:00 / 15-Jun-2021 12:30:00)\n', DV(1,:), DV(2,:));

C6 = datestr([738885;738886;738887;738888;738889;738900]);
fprintf('6x1 column = 6 dates: rows=%d (expect 6) r6=[%s] (expect 14-Jan-2023)\n', size(C6,1), C6(6,:));

% Single date / 1x6 date vector unchanged.
fprintf('single serial: [%s]  (expect 30-Dec-2022)\n', datestr(738885));
fprintf('single 1x6:    [%s]  (expect 28-Jul-2020)\n', datestr([2020 7 28 0 0 0]));
