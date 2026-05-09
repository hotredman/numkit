clear

import compat.*

% Arithmetic ops batch — audit ТЗ closure 2026-05-09. 10 functions.

fprintf('plus(2,3)         = %g\n', plus(2,3));
fprintf('minus(5,2)        = %g\n', minus(5,2));
fprintf('times(2,3)        = %g\n', times(2,3));
fprintf('rdivide(6,2)      = %g\n', rdivide(6,2));
fprintf('ldivide(2,6)      = %g  (=6/2)\n', ldivide(2,6));
fprintf('uminus(5)         = %g\n', uminus(5));
fprintf('uplus(-3)         = %g\n', uplus(-3));
fprintf('power(2, 10)      = %g\n', power(2,10));
fprintf('mpower(2, 3)      = %g\n', mpower(2,3));
fprintf('mtimes [1 2;3 4]*[5 6;7 8]:\n');
disp(mtimes([1 2; 3 4], [5 6; 7 8]));
