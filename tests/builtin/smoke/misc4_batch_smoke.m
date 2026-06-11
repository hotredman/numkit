clear

import compat.*

% Misc batch 4 — convert + intmax/intmin + collection helpers + meshgrid + misc.

fprintf('intmax(int8)  = %d, intmin(int8)  = %d\n', intmax("int8"), intmin("int8"));
fprintf('iscellstr({"a","b"}) = %d\n', iscellstr({"a","b"}));
fprintf('issorted([1 2 3]) = %d, issorted([3 1 2]) = %d\n', issorted([1 2 3]), issorted([3 1 2]));
fprintf('ismembertol([1.0001 2 3],[1 2 4],1e-3) = '); disp(ismembertol([1.0001 2 3],[1 2 4],1e-3));
[X,Y] = meshgrid(1:3, 1:2);
fprintf('meshgrid X:\n'); disp(X);
fprintf('meshgrid Y:\n'); disp(Y);
fprintf('nnz([1 0 2 0 3]) = %d\n', nnz([1 0 2 0 3]));
fprintf('nthroot(-27,3) = %g\n', nthroot(-27,3));
fprintf('pad("hi", 5) length = %d\n', strlength(pad("hi", 5)));
