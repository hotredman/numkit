clear

import compat.*

% calendar(year, month): 6x7 month matrix. Columns Sunday..Saturday, each
% day in its day-of-week column, weeks down rows, empty cells 0, 6 rows.
% Implemented 2026-05-30 (was an undefined function). vs MATLAB R2025b.

fprintf('=== December 2022 (starts Thursday) ===\n');
C = calendar(2022, 12);
disp(C);
fprintf('size = %dx%d (expect 6x7)\n', size(C,1), size(C,2));
fprintf('C(1,5) = %d (expect 1, day 1 on Thursday)\n', C(1,5));
fprintf('C(5,7) = %d (expect 31, last day)\n', C(5,7));
fprintf('sum    = %d (expect 496 = 1+2+...+31)\n', sum(C(:)));

fprintf('\n=== February 2024 (leap, ends Thursday) ===\n');
F = calendar(2024, 2);
fprintf('F(5,5) = %d (expect 29)\n', F(5,5));
fprintf('F(5,6) = %d (expect 0)\n', F(5,6));
fprintf('sum    = %d (expect 435 = 1+...+29)\n', sum(F(:)));
