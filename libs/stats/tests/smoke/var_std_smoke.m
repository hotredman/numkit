clear

import compat.*

A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11];
v = [2 5 3 7 4 6 NaN 8 1 9]';

fprintf('=== var basic ===\n');
disp(var(A));        fprintf('  expect [2.5 2.5 2.5]\n\n');
disp(var(A, 1));     fprintf('  expect [2 2 2]\n\n');

fprintf('=== "all" / vecdim ===\n');
fprintf('  var(A, 0, "all")  = %.4f (expect 8.5714)\n', var(A, 0, 'all'));
fprintf('  var(A, 0, [1 2])  = %.4f (expect 8.5714)\n\n', var(A, 0, [1 2]));

fprintf('=== weighted vector ===\n');
fprintf('  var([1:5], [1 2 1 2 1]) = %.4f (expect 1.7143)\n\n', ...
        var([1:5]', [1 2 1 2 1]'));

fprintf('=== nanflag ===\n');
fprintf('  var(v) default      = %.4f (expect NaN)\n', var(v));
fprintf('  var(v, 0, "omitnan") = %.4f (expect 7.5)\n', var(v, 0, 'omitnan'));

fprintf('\n=== std mirrors var (sqrt) ===\n');
disp(std(A));
fprintf('  expect [1.5811 1.5811 1.5811]\n');
fprintf('  std(A, 0, "all") = %.4f\n', std(A, 0, 'all'));
