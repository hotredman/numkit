clear

import compat.*

fprintf('=== normstat ===\n');

[m, v] = normstat(0, 1);
fprintf('  N(0,1)   : m=%g v=%g (expect 0 / 1)\n', m, v);

[m, v] = normstat([0 1 -2], [1 2 0.5]);
fprintf('  vector m = [%g %g %g] (expect [0 1 -2])\n', m(1), m(2), m(3));
fprintf('  vector v = [%g %g %g] (expect [1 4 0.25])\n', v(1), v(2), v(3));

[m, v] = normstat(0, [1 2 5]);
fprintf('  scalar mu + vector sigma:\n');
fprintf('    m=[%g %g %g] (expect [0 0 0])\n', m(1), m(2), m(3));
fprintf('    v=[%g %g %g] (expect [1 4 25])\n', v(1), v(2), v(3));

fprintf('\n--- invalid params ---\n');
fprintf('  sigma=0  : %g (expect NaN)\n', normstat(0,  0));
fprintf('  sigma<0  : %g (expect NaN)\n', normstat(0, -1));
