clear

import compat.*

% Misc batch 3 — interp + ind2sub + predicates3 + helpers.

fprintf('interp1([1 2 3],[10 20 30],1.5) = %g\n', interp1([1 2 3],[10 20 30],1.5));
fprintf('discretize([0.5 1.5 2.5],[0 1 2 3]) = '); disp(discretize([0.5 1.5 2.5],[0 1 2 3]));
[i, j] = ind2sub([3 4], 5);
fprintf('ind2sub([3 4], 5) = (%d, %d)\n', i, j);
fprintf('sub2ind([3 4], 2, 2) = %d\n', sub2ind([3 4], 2, 2));
fprintf('isfloat(3.14) = %d, isinteger(int8(5)) = %d\n', isfloat(3.14), isinteger(int8(5)));
fprintf('iskeyword("if") = %d\n', iskeyword("if"));
fprintf('isletter("abc 123"): '); disp(isletter("abc 123"));
fprintf('isspace("a b c"): '); disp(isspace("a b c"));
fprintf('inf > 0 = %d, isnan(nan) = %d\n', inf > 0, isnan(nan));
fprintf('idivide(int32(7), int32(2)) = %d\n', idivide(int32(7), int32(2)));
5;
fprintf('ans (after `5;`) = %g\n', ans);
