clear

import compat.*

% kstest / kstest2 — Tail aliases + Name-Value parsing.

x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]';
y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]';

fprintf('=== kstest2 ===\n');
[h, p, D] = kstest2(x, y);
fprintf('  default          : h=%d p=%.4f D=%.4f\n', h, p, D);
[h, p, D] = kstest2(x, y, 'Tail', 'larger');
fprintf('  Tail=larger      : h=%d p=%.4f D=%.4f (expect D=0)\n', h, p, D);
[h, p, D] = kstest2(x, y, 'Tail', 'smaller');
fprintf('  Tail=smaller     : h=%d p=%.4f D=%.4f\n', h, p, D);
[h, p, D] = kstest2(x, y, 'Alpha', 0.01, 'Tail', 'larger');
fprintf('  Alpha+Tail combo : h=%d p=%.4f D=%.4f\n', h, p, D);

fprintf('\n=== kstest ===\n');
[h, p, D] = kstest(x);
fprintf('  default vs N(0,1) : h=%d p=%.6f D=%.4f\n', h, p, D);
[h, p, D] = kstest(x, 'Tail', 'larger');
fprintf('  Tail=larger       : h=%d p=%.6f D=%.4f\n', h, p, D);
