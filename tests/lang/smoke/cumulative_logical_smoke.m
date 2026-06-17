clear

import compat.*

% bugs/builtin/cumulative-logical.md — cumsum/cumprod/cummax/cummin on logical.
% cumsum/cumprod PROMOTE logical -> double; cummax/cummin PRESERVE logical.

L = logical([1 0 1 1]);

ys = cumsum(L);
fprintf('cumsum(logical([1 0 1 1])) = [%g %g %g %g]   expect [1 1 2 3], islogical=%d (expect 0)\n', ...
        ys(1), ys(2), ys(3), ys(4), islogical(ys));

yp = cumprod(logical([1 1 0 1]));
fprintf('cumprod(logical([1 1 0 1])) = [%g %g %g %g]   expect [1 1 0 0], islogical=%d (expect 0)\n', ...
        yp(1), yp(2), yp(3), yp(4), islogical(yp));

ymx = cummax(logical([0 1 0 1]));
fprintf('cummax(logical([0 1 0 1])) = [%g %g %g %g]   expect [0 1 1 1], islogical=%d (expect 1)\n', ...
        ymx(1), ymx(2), ymx(3), ymx(4), islogical(ymx));

ymn = cummin(logical([1 1 0 1]));
fprintf('cummin(logical([1 1 0 1])) = [%g %g %g %g]   expect [1 1 0 0], islogical=%d (expect 1)\n', ...
        ymn(1), ymn(2), ymn(3), ymn(4), islogical(ymn));

% 2-D: column-wise (default) and along dim 2.
A = logical([1 0; 1 1]);
C = cumsum(A);
fprintf('cumsum(logical([1 0;1 1])) col = [%g %g; %g %g]   expect [1 0; 2 1]\n', ...
        C(1,1), C(1,2), C(2,1), C(2,2));
R = cumsum(A, 2);
fprintf('cumsum(...,2) row = [%g %g; %g %g]   expect [1 1; 1 2]\n', ...
        R(1,1), R(1,2), R(2,1), R(2,2));

% scalar + 'reverse'.
fprintf('cumsum(true) = %g   expect 1\n', cumsum(true));
rev = cumsum(L, 'reverse');
fprintf('cumsum(L,''reverse'') = [%g %g %g %g]   expect [3 2 2 1]\n', rev(1), rev(2), rev(3), rev(4));
