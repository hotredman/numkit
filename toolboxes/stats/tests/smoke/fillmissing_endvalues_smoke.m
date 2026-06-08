clear

import compat.*

% fillmissing(..., 'EndValues', ev) — DEEP-PROBE 2026-05-31.
% numkit previously had no 'EndValues' option (threw "Cannot convert
% char to scalar"). The option governs ONLY the endpoint missing
% entries — those before the first / after the last ORIGINAL good
% value. Interior missing runs are always filled by the method.
% Supported: 'extrap' (default), 'none', 'nearest', numeric constant.
% Reference: MATLAB R2025b.

a = [NaN NaN 3 5 NaN 9 NaN];

fprintf('=== linear, EndValues extrap (default) ===\n');
ex = fillmissing(a, 'linear');
fprintf('%g %g %g %g %g %g %g   (expect -1 1 3 5 7 9 11)\n', ...
        ex(1),ex(2),ex(3),ex(4),ex(5),ex(6),ex(7));

fprintf('\n=== linear, EndValues none ===\n');
en = fillmissing(a, 'linear', 'EndValues', 'none');
fprintf('%g %g %g %g %g %g %g   (expect NaN NaN 3 5 7 9 NaN)\n', ...
        en(1),en(2),en(3),en(4),en(5),en(6),en(7));

fprintf('\n=== linear, EndValues 0 ===\n');
ec = fillmissing(a, 'linear', 'EndValues', 0);
fprintf('%g %g %g %g %g %g %g   (expect 0 0 3 5 7 9 0)\n', ...
        ec(1),ec(2),ec(3),ec(4),ec(5),ec(6),ec(7));

fprintf('\n=== linear, EndValues nearest ===\n');
enr = fillmissing(a, 'linear', 'EndValues', 'nearest');
fprintf('%g %g %g %g %g %g %g   (expect 3 3 3 5 7 9 9)\n', ...
        enr(1),enr(2),enr(3),enr(4),enr(5),enr(6),enr(7));

fprintf('\n=== previous, EndValues -7 ===\n');
ep = fillmissing(a, 'previous', 'EndValues', -7);
fprintf('%g %g %g %g %g %g %g   (expect -7 -7 3 5 5 9 -7)\n', ...
        ep(1),ep(2),ep(3),ep(4),ep(5),ep(6),ep(7));

fprintf('\n=== matrix [NaN 10;2 NaN;NaN 30] linear none (per column) ===\n');
M = fillmissing([NaN 10; 2 NaN; NaN 30], 'linear', 'EndValues', 'none');
fprintf('col1 = %g %g %g  (expect NaN 2 NaN)\n', M(1,1), M(2,1), M(3,1));
fprintf('col2 = %g %g %g  (expect 10 20 30)\n', M(1,2), M(2,2), M(3,2));
