clear

import compat.*

% qmf: quadrature mirror filter — reverse + alternating sign flip.
% y(k) = (-1)^(k-1+p) * x(N-k+1)

fprintf('=== default p = 0 ===\n');
disp(qmf([1 2 3 4 5]));
fprintf('  expect: [5 -4 3 -2 1]\n\n');

fprintf('=== p = 1 (negate) ===\n');
disp(qmf([1 2 3 4 5], 1));
fprintf('  expect: [-5 4 -3 2 -1]\n\n');

fprintf('=== column preserves orientation ===\n');
y = qmf([1; 2; 3]);
fprintf('  size = %dx%d (expect 3x1)\n', size(y, 1), size(y, 2));
disp(y);

fprintf('=== qmf identity: qmf(x, 1) == -qmf(x) ===\n');
x = [1.5 -2 3 -4 5.5];
diff = max(abs(qmf(x, 1) + qmf(x)));
fprintf('  max diff: %.4e (expect 0)\n', diff);
