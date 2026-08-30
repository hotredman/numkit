clear

x = [3 4 5 4 5 6 7 5 4 3 5 6 4 3 5]';

fprintf('=== poissfit ===\n');
[lh, lci] = poissfit(x);
fprintf('  basic     : lh=%.4f  ci=[%.4f, %.4f]\n', lh, lci(1), lci(2));
fprintf('              (expect 4.6000 [3.5791, 5.8216])\n');

[lh, lci] = poissfit([0 0 0 0]');
fprintf('  all-zero  : lh=%g  ci=[%g, %.4f]\n', lh, lci(1), lci(2));
fprintf('              (expect 0 [0, 0.9222])\n');

[lh, lci] = poissfit(x, 0.01);
fprintf('  α=0.01    : lh=%.4f  ci=[%.4f, %.4f]\n', lh, lci(1), lci(2));
fprintf('              (expect 4.6000 [3.2988, 6.2282])\n');

[lh, lci] = poissfit([]);
fprintf('  empty     : lh=%g  ci=[%g, %g]  (expect NaN NaN NaN)\n', lh, lci(1), lci(2));
