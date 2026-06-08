clear
import compat.*

fprintf('=== isoutlier / rmoutliers ===\n');
x = [1 2 3 4 5 6 7 100];
m = isoutlier(x);
fprintf('  isoutlier([1..7 100]) = '); disp(m);
fprintf('  rmoutliers([1..7 100]) = '); disp(rmoutliers(x));

fprintf('\n=== fillmissing ===\n');
y = [1 2 NaN 4 5 NaN 7];
fprintf('  y = [1 2 NaN 4 5 NaN 7]\n');
fprintf('  fillmissing(y, ''previous'') = '); disp(fillmissing(y, 'previous'));
fprintf('  fillmissing(y, ''next'') = '); disp(fillmissing(y, 'next'));
fprintf('  fillmissing(y, ''constant'', 99) = '); disp(fillmissing(y, 'constant', 99));
fprintf('  fillmissing(y, ''mean'') (numkit ext) = '); disp(fillmissing(y, 'mean'));

fprintf('=== rmmissing ===\n');
fprintf('  rmmissing(y) = '); disp(rmmissing(y));

fprintf('=== standardizeMissing ===\n');
z = [1 2 -999 4 5 -999 7];
fprintf('  standardizeMissing(z, -999) = '); disp(standardizeMissing(z, -999));
