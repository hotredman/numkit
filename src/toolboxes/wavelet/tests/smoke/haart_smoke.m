clear

import compat.*

fprintf('=== haart level=1 ===\n');
[a, d] = haart([1 2 3 4 5 6 7 8], 1);
fprintf('  a = '); disp(a');
fprintf('  d = '); disp(d');

fprintf('=== default (cell-array detail) ===\n');
[a, d] = haart([1 2 3 4 5 6 7 8]);
fprintf('  a = %.4f, length(d) = %d\n', a, length(d));

fprintf('=== integer mode ===\n');
[a, d] = haart([10 20 30 40 50 60 70 80], 1, 'integer');
fprintf('  a = '); disp(a');
fprintf('  d = '); disp(d');
