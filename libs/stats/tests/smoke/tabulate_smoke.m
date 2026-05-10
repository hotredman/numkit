clear
import compat.*

fprintf('=== tabulate (frequency table) ===\n');

T = tabulate([1 2 3 2 1 4 1 5 5 5]);
fprintf('  tabulate([1 2 3 2 1 4 1 5 5 5]) (positive ints, dense):\n');
disp(T)
fprintf('  expect:\n     1     3    30\n     2     2    20\n     3     1    10\n     4     1    10\n     5     3    30\n');

T2 = tabulate([1 2 NaN 2 1]);
fprintf('\n  tabulate([1 2 NaN 2 1]) — NaN excluded:\n');
disp(T2)
fprintf('  expect:\n     1     2    50\n     2     2    50\n');

T3 = tabulate([3 5 3 1 5 5]);
fprintf('\n  tabulate([3 5 3 1 5 5]) — dense layout (zero rows for missing 2,4):\n');
disp(T3)
fprintf('  expect:\n     1     1    16.6667\n     2     0     0\n     3     2    33.3333\n     4     0     0\n     5     3    50\n');

T4 = tabulate([0.5 1.5 0.5 2.0]);
fprintf('\n  tabulate non-integer (sparse layout):\n');
disp(T4)
fprintf('  expect:\n     0.5    2    50\n     1.5    1    25\n     2.0    1    25\n');

Te = tabulate([]);
fprintf('  tabulate([]) shape: [%d %d] (expect [0 1])\n', size(Te,1), size(Te,2));
