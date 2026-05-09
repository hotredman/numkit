clear
import compat.*

fprintf('=== datevec (inverse of datenum) ===\n');

v = datevec(datenum(2026, 5, 9));
fprintf('  datevec(datenum(2026,5,9))           = [%d %d %d %d %d %g]\n', ...
        v(1), v(2), v(3), v(4), v(5), v(6));
fprintf('    expect [2026 5 9 0 0 0]\n');

v = datevec(datenum(2026, 5, 9, 12, 30, 45));
fprintf('  datevec(datenum(2026,5,9,12,30,45))  = [%d %d %d %d %d %g]\n', ...
        v(1), v(2), v(3), v(4), v(5), v(6));
fprintf('    expect [2026 5 9 12 30 45]\n');

v = datevec(719529);
fprintf('  datevec(719529) [Unix epoch]         = [%d %d %d %d %d %g]\n', ...
        v(1), v(2), v(3), v(4), v(5), v(6));
fprintf('    expect [1970 1 1 0 0 0]\n');

v = datevec(1);
fprintf('  datevec(1) [year zero]               = [%d %d %d %d %d %g]\n', ...
        v(1), v(2), v(3), v(4), v(5), v(6));
fprintf('    expect [0 1 1 0 0 0]\n');

v = datevec(0);
fprintf('  datevec(0) [edge]                    = [%d %d %d %d %d %g]\n', ...
        v(1), v(2), v(3), v(4), v(5), v(6));
fprintf('    expect [0 0 0 0 0 0]\n');

% Vector input -> Nx6 matrix
M = datevec([datenum(2026,5,9); datenum(2026,5,10); datenum(2026,5,11)]);
fprintf('  vec input shape: [%d %d]  (expect [3 6])\n', size(M,1), size(M,2));
fprintf('  M(1,1)=%d M(2,1)=%d M(3,3)=%d\n', M(1,1), M(2,1), M(3,3));

% Multi-output form
[Y, Mo, D, H, MI, S] = datevec(datenum(2026, 5, 9, 12, 30, 45.5));
fprintf('  [Y M D H MI S] = [%d %d %d %d %d %g]  (expect [2026 5 9 12 30 45.5])\n', ...
        Y, Mo, D, H, MI, S);

% Fractional day
v = datevec(datenum(2026, 5, 9) + 0.5);
fprintf('  datevec(d + 0.5)  = [%d %d %d %d %d %g]  (expect [.. 12 0 0])\n', ...
        v(1), v(2), v(3), v(4), v(5), v(6));

v = datevec(datenum(2026, 5, 9) + 0.25);
fprintf('  datevec(d + 0.25) = [%d %d %d %d %d %g]  (expect [.. 6 0 0])\n', ...
        v(1), v(2), v(3), v(4), v(5), v(6));
