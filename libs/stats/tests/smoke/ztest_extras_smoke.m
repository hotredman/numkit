clear

import compat.*

x = [1.2 2.4 3.1 4.5 5.0]';

fprintf('=== ztest Alpha NV ===\n');
[~,~,c95] = ztest(x, 3, 1, 'Alpha', 0.05);
[~,~,c99] = ztest(x, 3, 1, 'Alpha', 0.01);
fprintf('  CI95 width = %.4f  CI99 width = %.4f (expect 99 wider)\n', ...
    c95(2)-c95(1), c99(2)-c99(1));

fprintf('\n=== ztest Tail NV ===\n');
[h, p] = ztest(x, 3, 1, 'Tail', 'right');
fprintf('  right tail: h=%d p=%.4f\n', h, p);

fprintf('\n=== combined ===\n');
[h, p, ci, z] = ztest(x, 3, 1, 'Alpha', 0.01, 'Tail', 'both');
fprintf('  h=%d p=%.4f z=%.4f ci=[%.4f %.4f]\n', h, p, z, ci(1), ci(2));
