clear
import compat.*

% repelem with per-element / per-row / per-column counts.

% Scalar count (fast path).
fprintf('scalar : %s\n', mat2str(repelem([1 2 3], 2)));      % expect [1 1 2 2 3 3]

% Per-element count vector.
fprintf('counts : %s\n', mat2str(repelem([1 2 3], [1 2 3]))); % expect [1 2 2 3 3 3]

% Zero count drops an element; column stays a column.
c = repelem([1; 2; 3], [2 0 1]);
fprintf('col    : %s (col? %d)\n', mat2str(c(:).'), iscolumn(c)); % expect [1 1 3], col 1

% Matrix: r scalar, c vector.
B1 = repelem([1 2; 3 4], 2, [1 2]);
fprintf('r2,c[1 2] size %dx%d B(1,2)=%g B(4,3)=%g\n', ...
        size(B1,1), size(B1,2), B1(1,2), B1(4,3));   % expect 4x3, 2, 4

% Matrix: r vector, c scalar.
B2 = repelem([1 2; 3 4], [2 1], 3);
fprintf('r[2 1],c3 size %dx%d B(2,1)=%g B(3,6)=%g\n', ...
        size(B2,1), size(B2,2), B2(2,1), B2(3,6));   % expect 3x6, 1, 4
