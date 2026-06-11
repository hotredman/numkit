clear

import compat.*

fprintf('=== chi2stat ===\n');

[m, v] = chi2stat(5);
fprintf('  k=5 : m=%g v=%g (expect 5 / 10)\n', m, v);

[m, v] = chi2stat([1 5 10 30]);
fprintf('  vector m = [%g %g %g %g] (expect [1 5 10 30])\n', m(1), m(2), m(3), m(4));
fprintf('  vector v = [%g %g %g %g] (expect [2 10 20 60])\n', v(1), v(2), v(3), v(4));

fprintf('\n--- edges ---\n');
fprintf('  k=0  : %g (expect NaN; moments undefined)\n', chi2stat(0));
fprintf('  k<0  : %g (expect NaN)\n', chi2stat(-1));
