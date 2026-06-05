clear

import compat.*

% Construction + search/sort + mod + booleans batch — spec closure 2026-05-09.

fprintf('zeros(2,3): '); disp(zeros(2,3));
fprintf('ones(2,3):  '); disp(ones(2,3));
fprintf('eye(3):     '); disp(eye(3));
fprintf('linspace(0,1,5) = '); disp(linspace(0,1,5));
fprintf('logspace(0,2,3) = '); disp(logspace(0,2,3));
fprintf('repmat([1 2],2,2) = '); disp(repmat([1 2],2,2));
fprintf('sort([3 1 4 1 5]) = '); disp(sort([3 1 4 1 5]));
fprintf('find([0 1 0 1 1]) = '); disp(find([0 1 0 1 1]));
fprintf('unique([1 2 1 3]) = '); disp(unique([1 2 1 3]));
fprintf('mod(10,3) = %g, mod(-10,3) = %g\n', mod(10,3), mod(-10,3));
fprintf('rem(10,3) = %g, rem(-10,3) = %g\n', rem(10,3), rem(-10,3));
fprintf('true = %d, false = %d\n', true, false);
