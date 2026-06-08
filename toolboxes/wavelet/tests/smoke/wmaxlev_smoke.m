clear

import compat.*

% wmaxlev: maximum decomposition level for a signal/wavelet pair.
% L = floor(log2(N / (Lf - 1))) where Lf = filter length.

fprintf('  wmaxlev(64, "db2")    = %d (expect 4)\n', wmaxlev(64, 'db2'));
fprintf('  wmaxlev(64, "db1")    = %d (expect 6)\n', wmaxlev(64, 'db1'));
fprintf('  wmaxlev(1024, "db4")  = %d (expect 7)\n', wmaxlev(1024, 'db4'));
fprintf('  wmaxlev([8 8], "db1") = %d (expect 3, uses min(N))\n', wmaxlev([8 8], 'db1'));
fprintf('  wmaxlev(2, "db1")     = %d (expect 1)\n', wmaxlev(2, 'db1'));
fprintf('  wmaxlev(1, "db1")     = %d (expect 0)\n', wmaxlev(1, 'db1'));
