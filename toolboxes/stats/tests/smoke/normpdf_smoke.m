clear

import compat.*

fprintf('=== normpdf ===\n');
fprintf('  N(0,1) at 0  = %.6f (expect 0.398942 = 1/sqrt(2*pi))\n', normpdf(0));
fprintf('  N(0,1) at 1  = %.6f (expect 0.241971)\n', normpdf(1));

y = normpdf([-2 -1 0 1 2]);
fprintf('\n  vector x [-2..2] (default):\n');
fprintf('    [%.4f %.4f %.4f %.4f %.4f]\n', y(1), y(2), y(3), y(4), y(5));
fprintf('    expect [0.0540 0.2420 0.3989 0.2420 0.0540] (symmetric)\n');

fprintf('\n  N(2, 0.5) at peak+sigma (x=2.5) = %.4f (expect 0.4839)\n', normpdf(2.5, 2, 0.5));

fprintf('\n--- invalid params ---\n');
fprintf('  sigma=0  : %g (expect NaN)\n', normpdf(1, 0, 0));
fprintf('  sigma<0  : %g (expect NaN)\n', normpdf(1, 0, -1));
