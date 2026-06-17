clear

import compat.*

% etime(t2, t1): elapsed seconds between two date vectors [Y M D H MI S].
% Each input is one 6-element row, or an N-by-6 matrix of rows; the result
% is an N-by-1 column of elapsed seconds. Calendar-aware (month/year/leap
% boundaries handled). Implemented 2026-05-30 (was an undefined function).
% vs MATLAB R2025b.

fprintf('=== scalar date-vector pairs ===\n');
fprintf('frac second : %.10g (expect 0.5)\n', etime([2026 5 30 12 0 30.5],[2026 5 30 12 0 30]));
fprintf('month cross : %g (expect 86400)\n', etime([2026 6 1 0 0 0],[2026 5 31 0 0 0]));
fprintf('year cross  : %g (expect 86400)\n', etime([2027 1 1 0 0 0],[2026 12 31 0 0 0]));
fprintf('leap cross  : %g (expect 86400)\n', etime([2024 3 1 0 0 0],[2024 2 29 0 0 0]));
fprintf('negative    : %g (expect -3600)\n', etime([2026 5 30 11 0 0],[2026 5 30 12 0 0]));

fprintf('\n=== matrix form (each row a date vector) ===\n');
r = etime([2026 5 30 0 0 10; 2026 5 30 0 0 20],[2026 5 30 0 0 0; 2026 5 30 0 0 0]);
fprintf('size = %dx%d (expect 2x1), iscolumn=%d\n', size(r,1), size(r,2), iscolumn(r));
fprintf('r = %g %g (expect 10 20)\n', r(1), r(2));

fprintf('\n=== single-row broadcast ===\n');
b = etime([2026 5 30 0 0 1; 2026 5 30 0 0 2; 2026 5 30 0 0 3],[2026 5 30 0 0 0]);
fprintf('b = %g %g %g (expect 1 2 3)\n', b(1), b(2), b(3));
