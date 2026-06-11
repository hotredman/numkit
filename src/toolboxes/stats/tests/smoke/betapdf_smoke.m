clear

import compat.*

fprintf('=== betapdf ===\n');
fprintf('  scalar (0.5, 2, 3)     = %.4f (expect 1.5000)\n', betapdf(0.5, 2, 3));

y = betapdf([0.1 0.5 0.9]', 2, 3);
fprintf('  vector [0.1 0.5 0.9]'' = [%.3f %.3f %.3f] (expect [0.972 1.500 0.108])\n', y(1), y(2), y(3));

fprintf('\n--- out-of-support edges (Beta domain is (0,1)) ---\n');
fprintf('  x=-0.1 : %g (expect 0)\n', betapdf(-0.1, 2, 3));
fprintf('  x=0    : %g (expect 0)\n', betapdf( 0.0, 2, 3));
fprintf('  x=1    : %g (expect 0)\n', betapdf( 1.0, 2, 3));
fprintf('  x=1.5  : %g (expect 0)\n', betapdf( 1.5, 2, 3));

fprintf('\n--- invalid params ---\n');
fprintf('  a=0   : %g (expect NaN)\n', betapdf(0.5,  0, 3));
fprintf('  b=0   : %g (expect NaN)\n', betapdf(0.5,  2, 0));
fprintf('  a<0   : %g (expect NaN)\n', betapdf(0.5, -1, 3));
