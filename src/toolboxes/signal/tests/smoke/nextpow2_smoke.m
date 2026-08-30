clear

fprintf('=== nextpow2 ===\n');
fprintf('  vector       : '); disp(nextpow2([1 5 100 1024 1025]));
fprintf('               (expect: 0  3  7 10 11)\n');
fprintf('  zero         : %g (expect 0)\n', nextpow2(0));
fprintf('  negative     : nextpow2(-100) = %g (expect 7 — uses |x|)\n', nextpow2(-100));
fprintf('  fractional   : nextpow2(7.5) = %g, nextpow2(0.5) = %g, nextpow2(0.25) = %g\n', ...
    nextpow2(7.5), nextpow2(0.5), nextpow2(0.25));
fprintf('  complex      : nextpow2(3+4i) = %g (expect 3 — |z|=5)\n', nextpow2(3+4i));
fprintf('  NaN          : %g (NaN)\n', nextpow2(NaN));
fprintf('  +Inf, -Inf   : %g, %g (both +Inf)\n', nextpow2(Inf), nextpow2(-Inf));
