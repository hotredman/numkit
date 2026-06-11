clear

import compat.*

x = [2 5 3 7 4 6 8 1 9 5]';

fprintf('=== unifit ===\n');
[a, b, aci, bci] = unifit(x);
fprintf('  basic   : a=%g b=%g aci=[%.4f, %g] bci=[%g, %.4f]\n', ...
    a, b, aci(1), aci(2), bci(1), bci(2));
fprintf('            (expect a=1, b=9, aci=[-1.7943, 1], bci=[9, 11.7943])\n');

[a, b, aci, bci] = unifit(x, 0.01);
fprintf('  α=0.01  : aci=[%.4f, %g] bci=[%g, %.4f]\n', ...
    aci(1), aci(2), bci(1), bci(2));
fprintf('            (expect aci=[-3.6791, 1], bci=[9, 13.6791])\n');

[a, b, aci, bci] = unifit([5]);
fprintf('  one-pt  : a=%g b=%g aci=[%g, %g] bci=[%g, %g] (expect all 5)\n', ...
    a, b, aci(1), aci(2), bci(1), bci(2));

[a, b, aci, bci] = unifit([]);
fprintf('  empty   : a=%g b=%g aci=[%g, %g] (NaN — numkit convention)\n', ...
    a, b, aci(1), aci(2));
