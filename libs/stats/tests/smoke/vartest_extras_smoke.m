clear

import compat.*

x = [1.2 2.4 3.1 4.5 5.0]';
y = [0.8 1.9 2.7 4.0 4.5]';

fprintf('=== vartest Alpha NV ===\n');
[~,~,c95] = vartest(x, 1, 'Alpha', 0.05);
[~,~,c99] = vartest(x, 1, 'Alpha', 0.01);
fprintf('  CI95 = [%.4f %.4f]  width = %.4f\n', c95(1), c95(2), c95(2)-c95(1));
fprintf('  CI99 = [%.4f %.4f]  width = %.4f\n', c99(1), c99(2), c99(2)-c99(1));

fprintf('\n=== vartest Tail NV ===\n');
[h, p] = vartest(x, 1, 'Tail', 'right');
fprintf('  right tail: h=%d p=%.4f\n', h, p);

fprintf('\n=== vartest2 NV ===\n');
[h, p, ci, F] = vartest2(x, y, 'Alpha', 0.01, 'Tail', 'both');
fprintf('  h=%d p=%.4f F=%.4f ci=[%.4f %.4f]\n', h, p, F, ci(1), ci(2));
