clear
import compat.*
% int2str(X) on vectors & matrices. DEEP-PROBE 2026-05-31: was SCALAR-ONLY
% (threw "only scalar inputs are supported"). MATLAB rounds each element
% half-away-from-zero, formats with field width digits(max|rounded|)+2, and
% strips common leading blank columns.

fprintf('row:        [%s]  (expect 1  2  3)\n', int2str([1 2 3]));
fprintf('row width:  [%s]  (expect 10  200    3)\n', int2str([10 200 3]));
fprintf('row round:  [%s]  (expect 2  3 -3)\n', int2str([1.5 2.5 -2.5]));
fprintf('row neg:    [%s]  (expect -5  10   3)\n', int2str([-5 10 3]));
fprintf('row frac:   [%s]  (expect 3 -3)\n', int2str([3.4 -2.6]));

M = int2str([1 2;30 4]);
fprintf('matrix: rows=%d (expect 2) cols=%d (expect 6)\n', size(M,1), size(M,2));
fprintf('  row1=[%s] (expect " 1   2")  row2=[%s] (expect "30   4")\n', M(1,:), M(2,:));

fprintf('scalar:     [%s]  (expect 3, round half away)\n', int2str(2.5));
