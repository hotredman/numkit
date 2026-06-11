clear

import compat.*

fprintf('=== fstat ===\n');

[m, v] = fstat(5, 10);
fprintf('  F(5,10)  : m=%.4f v=%.4f (expect 1.2500 / 1.3542)\n', m, v);

% Vector broadcast — v2=3 gives variance NaN (need v2>4)
[m, v] = fstat([5 5 5], [3 5 10]);
fprintf('  vec m    = [%.4f %.4f %.4f] (expect [3.0000 1.6667 1.2500])\n', m(1), m(2), m(3));
fprintf('  vec v    = [%g %.4f %.4f] (expect [NaN 8.8889 1.3542])\n', v(1), v(2), v(3));

fprintf('\n--- regimes ---\n');
[m, v] = fstat(5, 2);
fprintf('  v2=2     : m=%g v=%g (expect NaN NaN; mean needs v2>2)\n', m, v);

fprintf('\n--- invalid params ---\n');
fprintf('  v1=0  : %g (expect NaN)\n', fstat( 0, 10));
fprintf('  v2=0  : %g (expect NaN)\n', fstat( 5,  0));
fprintf('  v1<0  : %g (expect NaN)\n', fstat(-1, 10));
