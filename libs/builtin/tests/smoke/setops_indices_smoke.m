clear

import compat.*

% intersect/union/setdiff index outputs (ia/ib). Bug fixed 2026-05-30:
% these were 1-output only — [c,ia,ib]=intersect(...) errored. MATLAB
% returns the source indices as column vectors. vs MATLAB R2025b.

fprintf('=== intersect [c,ia,ib] ===\n');
[c, ia, ib] = intersect([3 1 2 5], [2 4 1]);
fprintf('c=%s ia=%s ib=%s (expect [1 2] [2;3] [3;1])\n', mat2str(c), mat2str(ia), mat2str(ib));

fprintf('\n=== setdiff [d,ia] ===\n');
[d, da] = setdiff([3 1 2 5], [2 4 1]);
fprintf('d=%s ia=%s (expect [3 5] [1;4])\n', mat2str(d), mat2str(da));

fprintf('\n=== union [u,ia,ib] ===\n');
[u, ua, ub] = union([3 1 2], [2 4 1]);
fprintf('u=%s ia=%s ib=%s (expect [1 2 3 4] [2;3;1] [2])\n', mat2str(u), mat2str(ua), mat2str(ub));

fprintf('\n=== index vectors are columns ===\n');
fprintf('iscolumn(ia)=%d iscolumn(da)=%d iscolumn(ua)=%d\n', iscolumn(ia), iscolumn(da), iscolumn(ua));

fprintf('\n=== 1-output forms unchanged ===\n');
fprintf('%s | %s | %s\n', mat2str(intersect([3 1 2 5],[2 4 1])), ...
        mat2str(union([3 1 2],[2 4 1])), mat2str(setdiff([3 1 2 5],[2 4 1])));
