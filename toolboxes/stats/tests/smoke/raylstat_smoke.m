clear

import compat.*

fprintf('=== raylstat ===\n');
[m, v] = raylstat(2);
fprintf('  b=2 : m=%.4f v=%.4f (expect 2.5066 / 1.7168)\n', m, v);
[m, v] = raylstat([1 2 3]);
fprintf('  vec : m=[%.4f %.4f %.4f]\n', m(1), m(2), m(3));
fprintf('  edges: b=0 → %g, b<0 → %g (both expect NaN)\n', raylstat(0), raylstat(-1));
