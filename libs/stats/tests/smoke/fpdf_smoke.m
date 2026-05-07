clear

import compat.*

fprintf('=== fpdf ===\n');
fprintf('  scalar (2, 5, 10)  = %.6f (expect 0.162006)\n', fpdf(2, 5, 10));

y = fpdf([0.5 1 2 5]', 5, 10);
fprintf('\n  vector x [0.5 1 2 5]'' (v1=5, v2=10):\n');
fprintf('    [%.6f %.6f %.6f %.6f]\n', y(1), y(2), y(3), y(4));
fprintf('    expect [0.687607 0.495480 0.162006 0.009631]\n');

fprintf('\n--- density at x=0 (regime depends on v1) ---\n');
fprintf('  v1=2, v2=10  : %.4f (expect 1.0; finite)\n', fpdf(0, 2, 10));
fprintf('  v1=1, v2=10  : %g (expect Inf; heavy at origin)\n', fpdf(0, 1, 10));
fprintf('  v1=5, v2=10  : %g (expect 0; v1>2)\n', fpdf(0, 5, 10));

fprintf('\n--- invalid params ---\n');
fprintf('  v1=0  : %g (expect NaN)\n', fpdf(2, 0, 10));
fprintf('  v2=0  : %g (expect NaN)\n', fpdf(2, 5, 0));
fprintf('  v1<0  : %g (expect NaN)\n', fpdf(2, -1, 10));
