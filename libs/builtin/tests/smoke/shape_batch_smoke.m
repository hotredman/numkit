clear

import compat.*

% Shape ops batch — audit ТЗ closure 2026-05-09. 16 functions.

A = [1 2 3; 4 5 6];
fprintf('size(A)         = '); disp(size(A));
fprintf('numel(A)        = %d\n', numel(A));
fprintf('length(A)       = %d\n', length(A));
fprintf('ndims(A)        = %d\n', ndims(A));

B = reshape(1:12, 3, 4);
fprintf('reshape(1:12,3,4) col-1: '); disp(B(:,1)');

fprintf('cat(2,[1;2],[3;4]) = '); disp(cat(2,[1;2],[3;4]));
fprintf('horzcat([1;2],[3;4]) = '); disp(horzcat([1;2],[3;4]));
fprintf('vertcat([1 2],[3 4]) = '); disp(vertcat([1 2],[3 4]));

fprintf('permute(A,[2 1]) = '); disp(permute(A,[2 1]));
fprintf('circshift(1:5,2) = '); disp(circshift(1:5,2));
fprintf('fliplr([1 2 3])  = '); disp(fliplr([1 2 3]));
fprintf('flipud([1;2;3])  = '); disp(flipud([1;2;3]));
fprintf('rot90([1 2;3 4]) = '); disp(rot90([1 2;3 4]));
fprintf('flip([1 2 3 4])  = '); disp(flip([1 2 3 4]));
