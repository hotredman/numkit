clear

import compat.*

% Predicates2 + set ops + format/transpose batch — spec closure 2026-05-09.

fprintf('isempty([])  = %d, isempty([1]) = %d\n', isempty([]), isempty([1]));
fprintf('isvector([1 2 3]) = %d, ismatrix([1;2]) = %d\n', isvector([1 2 3]), ismatrix([1;2]));
fprintf('isnumeric(5) = %d, isreal(1+1i) = %d\n', isnumeric(5), isreal(1+1i));
fprintf('isnan([1 NaN 2]): '); disp(isnan([1 NaN 2]));
fprintf('union([1 2 3],[3 4 5]) = '); disp(union([1 2 3],[3 4 5]));
fprintf('intersect([1 2 3],[2 4]) = '); disp(intersect([1 2 3],[2 4]));
fprintf('ismember([1 2 3 4],[2 4]) = '); disp(ismember([1 2 3 4],[2 4]));
fprintf('sprintf(''%%d'', 42) = "%s"\n', sprintf('%d', 42));
fprintf('num2str(3.14) = "%s"\n', num2str(3.14));
fprintf('str2double("3.14") = %g\n', str2double('3.14'));
fprintf('transpose([1 2 3]) = '); disp(transpose([1 2 3]));
