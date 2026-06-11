clear

import compat.*

% num2cell(A, dims) — DEEP-PROBE 2026-05-31. The dimension-collapsing form
% was unimplemented (num2cell(A,dim) threw "Index exceeds array size 1");
% only the element-wise num2cell(A) worked. MATLAB collapses the listed
% dims into each cell: the result cell has size(A) with the collapsed dims
% set to 1, and each cell holds the corresponding sub-array.
% Reference: MATLAB R2025b.

A = [1 2 3; 4 5 6];

fprintf('=== num2cell(A,2): collapse cols -> 2x1 cell of rows ===\n');
c2 = num2cell(A, 2);
fprintf('size %dx%d (expect 2x1); c2{1} = [%g %g %g] (expect 1 2 3); c2{2}(3) = %g (expect 6)\n', ...
        size(c2,1), size(c2,2), c2{1}(1), c2{1}(2), c2{1}(3), c2{2}(3));

fprintf('\n=== num2cell(A,1): collapse rows -> 1x3 cell of columns ===\n');
c1 = num2cell(A, 1);
fprintf('size %dx%d (expect 1x3); c1{1} = [%g; %g] (expect 1;4); c1{3}(2) = %g (expect 6)\n', ...
        size(c1,1), size(c1,2), c1{1}(1), c1{1}(2), c1{3}(2));

fprintf('\n=== num2cell(A,[1 2]): whole matrix in a 1x1 cell ===\n');
cb = num2cell(A, [1 2]);
fprintf('numel %g (expect 1); cb{1}(2,3) = %g (expect 6)\n', numel(cb), cb{1}(2,3));

fprintf('\n=== num2cell(A,3): trivial singleton -> element-wise 2x3 ===\n');
c3 = num2cell(A, 3);
fprintf('size %dx%d (expect 2x3); c3{2,2} = %g (expect 5)\n', size(c3,1), size(c3,2), c3{2,2});

fprintf('\n=== element-wise num2cell(A) unchanged ===\n');
ce = num2cell(A);
fprintf('size %dx%d (expect 2x3); ce{1,2} = %g (expect 2)\n', size(ce,1), size(ce,2), ce{1,2});
