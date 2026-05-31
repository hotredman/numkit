clear
import compat.*

[m, v] = nctstat(10, 2);
fprintf('nctstat(10, 2)  : m=%.10f  v=%.10f  (expect 2.1674446159, 1.5521838371)\n', m, v);

[m, v] = nctstat(5, 1.5);
fprintf('nctstat(5, 1.5) : m=%.10f  v=%.10f  (expect 1.7841241162, 2.2335678048)\n', m, v);

x = nctinv(0.3, 10, 2);
fprintf('\nnctinv(0.3, 10, 2)        = %.10f  (expect 1.4856759815)\n', x);
fprintf('  round-trip nctcdf       = %.10f  (expect 0.3)\n', nctcdf(x, 10, 2));

x2 = nctinv([0.1 0.5 0.9], 10, 2);
fprintf('\nnctinv([0.1 0.5 0.9], 10, 2):\n');
disp(x2);
fprintf('   expect: [0.7200 2.0537 3.7466]\n');

% δ = 0 limit: nctinv = tinv
fprintf('\nDelta=0 limit:\n');
fprintf('  nctinv(0.7, 10, 0) = %.12f\n', nctinv(0.7, 10, 0));
fprintf('   tinv(0.7, 10)     = %.12f  (should match)\n', tinv(0.7, 10));
