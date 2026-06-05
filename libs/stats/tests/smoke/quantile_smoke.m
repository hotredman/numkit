clear

import compat.*

% quantile / prctile / iqr — joint MATLAB R2025b compliance smoke
%.

v10 = [1:10]';
A   = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11];

fprintf('=== quantile midpoint default (R2007a) ===\n');
fprintf('  q(v10, 0.25) = %.4f (expect 3)\n', quantile(v10, 0.25));
fprintf('  q(v10, 0.5)  = %.4f (expect 5.5)\n', quantile(v10, 0.5));
fprintf('  q(v10, 0.75) = %.4f (expect 8)\n\n', quantile(v10, 0.75));

fprintf('=== Methods (override default) ===\n');
fprintf('  midpoint:  %.4f (expect 3)\n',    quantile(v10, 0.25, 'Method', 'midpoint'));
fprintf('  inclusive: %.4f (expect 3.25)\n', quantile(v10, 0.25, 'Method', 'inclusive'));
fprintf('  exclusive: %.4f (expect 2.75)\n\n', quantile(v10, 0.25, 'Method', 'exclusive'));

fprintf('=== "all" + vecdim flatten ===\n');
fprintf('  q(A, 0.5, "all")  = %.4f (expect 6)\n', quantile(A, 0.5, 'all'));
fprintf('  q(A, 0.5, [1 2])  = %.4f (expect 6)\n\n', quantile(A, 0.5, [1 2]));

fprintf('=== prctile ===\n');
disp(prctile(v10, [25 75]));
fprintf('  expect [3 8]\n\n');

fprintf('=== iqr ===\n');
fprintf('  iqr(v10)        = %.4f (expect 5)\n', iqr(v10));
fprintf('  iqr(A, "all")   = %.4f (expect 4)\n', iqr(A, 'all'));
disp(iqr(A));
fprintf('  expect rows: [2.5 2.5 2.5]\n');
