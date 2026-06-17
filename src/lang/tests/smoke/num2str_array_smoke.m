clear
import compat.*
% num2str(X) / num2str(X,N) on vectors & matrices (no explicit format).
% DEEP-PROBE 2026-05-31: these forms were SCALAR-ONLY (threw "Cannot convert
% double to scalar"); only num2str(X,FMT) handled arrays. MATLAB synthesises
% a column format and returns a (possibly multi-row) char matrix.

fprintf('row int:        [%s]  (expect 1  2  3)\n', num2str([1 2 3]));
fprintf('row int width:  [%s]  (expect 10  200    3)\n', num2str([10 200 3]));
fprintf('row frac:       [%s]  (expect 1.5        2.25           3)\n', num2str([1.5 2.25 3]));
fprintf('row big:        [%s]  (expect 12345.6               2)\n', num2str([12345.6 2]));
fprintf('row neg int:    [%s]  (expect -5  10)\n', num2str([-5 10]));
fprintf('row neg frac:   [%s]  (expect -1.5           2)\n', num2str([-1.5 2]));
fprintf('N=3 form:       [%s]  (expect 1         2         3)\n', num2str([1 2 3], 3));
fprintf('pi vec:         [%s]  (expect 3.1416      6.2832)\n', num2str(pi*[1 2]));

M = num2str([1 2;3 4]);
fprintf('matrix: rows=%d (expect 2) cols=%d (expect 4)\n', size(M,1), size(M,2));
fprintf('  row1=[%s] (expect 1  2)  row2=[%s] (expect 3  4)\n', M(1,:), M(2,:));

fprintf('scalar still:   [%s]  (expect 3.1416)\n', num2str(3.14159));
