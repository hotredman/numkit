clear

import compat.*

fprintf('=== tstat ===\n');
[m, v] = tstat(5);
fprintf('  nu=5  : m=%g v=%g (expect 0, 1.6667)\n', m, v);
[m, v] = tstat(2);
fprintf('  nu=2  : m=%g v=%g (expect 0, NaN; var needs nu>2)\n', m, v);
fprintf('  nu=1  : %g (expect NaN; Cauchy has no mean)\n', tstat(1));
fprintf('  nu=0  : %g (expect NaN)\n', tstat(0));
fprintf('  nu<0  : %g (expect NaN)\n', tstat(-1));
[m, v] = tstat([3 5 10]);
fprintf('  vector: v=[%g %g %g] (expect [3 1.6667 1.25])\n', v(1), v(2), v(3));
