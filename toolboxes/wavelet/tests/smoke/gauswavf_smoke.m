clear

import compat.*

% gauswavf — real Gaussian wavelet (analytical L^2 normalisation).

[psi1, x] = gauswavf(-5, 5, 11);
fprintf('p=1 (default):\n  psi = '); disp(psi1');
fprintf('  expect at t=-1: 0.6572, t=+1: -0.6572\n\n');

[psi2, ~] = gauswavf(-5, 5, 11, 2);
fprintf('p=2:\n  psi = '); disp(psi2');
fprintf('  expect peak at 0: 1.0314\n');

[psi4, ~] = gauswavf(-5, 5, 11, 4);
fprintf('p=4 peak: %.4f (expect 1.0461)\n', psi4(6));
