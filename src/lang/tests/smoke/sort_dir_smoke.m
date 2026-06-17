clear
import compat.*
% sort — direction ('descend') + NaN placement (MATLAB R2025b).
[sa, ia] = sort([3 1 NaN 2]);
fprintf('ascend : %g %g %g %g  idx %g %g %g %g (expect 1 2 3 NaN / 2 4 1 3)\n', sa(1),sa(2),sa(3),sa(4), ia(1),ia(2),ia(3),ia(4));

[sd, id] = sort([3 1 NaN 2], 'descend');
fprintf('descend: %g %g %g %g  idx %g %g %g %g (expect NaN 3 2 1 / 3 1 4 2)\n', sd(1),sd(2),sd(3),sd(4), id(1),id(2),id(3),id(4));

m = sort([3 5; 1 2; 8 4], 1, 'descend');
fprintf('matrix col-descend: col1 [%g %g %g] col2 [%g %g %g] (expect 8 3 1 / 5 4 2)\n', m(1,1),m(2,1),m(3,1), m(1,2),m(2,2),m(3,2));

% Complex sort: by magnitude |z|, ties by phase angle arg(z). MATLAB R2025b.
[cz, ci] = sort([3+4i 1 5i]);
fprintf('\ncomplex: sort([3+4i 1 5i]) = %s (expect [1 3+4i 5i])\n', mat2str(cz));
fprintf('  idx = [%g %g %g] (expect 2 1 3)\n', ci(1), ci(2), ci(3));
cd = sort([3+4i 1 5i], 'descend');
fprintf('  descend = %s (expect [5i 3+4i 1])\n', mat2str(cd));
ct = sort([2+0i, -2, 1i, -1i]);
fprintf('  ties-by-angle = %s (expect [-1i 1i 2 -2])\n', mat2str(ct));
