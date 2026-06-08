clear
import compat.*

fprintf('=== datenum (MATLAB serial date number from components) ===\n');

% 3-arg form
fprintf('  datenum(2026,5,9)              = %.6f  (expect 740111)\n', ...
        datenum(2026, 5, 9));
fprintf('  datenum(1970,1,1)              = %.6f  (expect 719529)\n', ...
        datenum(1970, 1, 1));
fprintf('  datenum(0,1,1)                 = %.6f  (expect 1)\n', ...
        datenum(0, 1, 1));
fprintf('  datenum(0,1,0)                 = %.6f  (expect 0)\n', ...
        datenum(0, 1, 0));

% 6-arg form
fprintf('  datenum(2026,5,9,12,30,45)     = %.6f  (expect 740111.521354)\n', ...
        datenum(2026, 5, 9, 12, 30, 45));

% Single-arg row vector
fprintf('  datenum([2026 5 9])            = %.6f  (expect 740111)\n', ...
        datenum([2026 5 9]));
fprintf('  datenum([2026 5 9 12 30 45])   = %.6f  (expect 740111.521354)\n', ...
        datenum([2026 5 9 12 30 45]));

% Single-arg matrix N×3
M = [2026 1 1; 2026 2 1; 2026 3 1];
y = datenum(M);
fprintf('  datenum([2026 1 1; 2026 2 1; 2026 3 1]) = [%.0f %.0f %.0f]\n', ...
        y(1), y(2), y(3));
fprintf('    expect [739983 740014 740042]\n');

% Vectorised 3-arg form
yy = datenum([2026; 2027; 2028], [1; 2; 3], [1; 15; 28]);
fprintf('  datenum(col, col, col) → [%.0f %.0f %.0f]\n', yy(1), yy(2), yy(3));
fprintf('    expect [739983 740393 740800]\n');

% Month/day overflow
fprintf('  datenum(2026,13,9)             = %.0f  (expect 740356, = 2027-01-09)\n', ...
        datenum(2026, 13, 9));
fprintf('  datenum(2026,2,30)             = %.0f  (expect 740043, = 2026-03-02)\n', ...
        datenum(2026, 2, 30));
