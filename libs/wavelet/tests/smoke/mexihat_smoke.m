clear

import compat.*

fprintf('=== mexihat ===\n');
[psi, x] = mexihat(-5, 5, 8);
fprintf('  mexihat(-5, 5, 8):\n');
fprintf('    psi: '); fprintf('%.4g ', psi); fprintf('\n');
fprintf('    x  : '); fprintf('%g ', x); fprintf('\n');
fprintf('  expect psi(1) = -0.0000776 (tiny edge), psi(4) = 0.3292 (rising)\n');

[psi16, x16] = mexihat(-5, 5, 16);
fprintf('  mexihat(-5, 5, 16): peak at center ≈ %.4f\n', psi16(8));

[psi_a, x_a] = mexihat(0, 5, 16);
fprintf('  mexihat(0, 5, 16): start = %.4f (peak val 2/(√3·π^¼))\n', psi_a(1));
