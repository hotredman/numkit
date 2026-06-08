clear

import compat.*

% cool — cyan-to-magenta colormap.

fprintf('--- size(cool()) ---\n');
c = cool();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(c)));

fprintf('\n--- cool(5) ---\n');
disp(cool(5));
fprintf('  expect: r = 0..1 step 0.25, g = 1-r, b = 1\n');

fprintf('\n--- cool(1) ---\n');
disp(cool(1));
fprintf('  expect [0 1 1]\n');

fprintf('\n--- cool(0) size ---\n');
fprintf('size cool(0)  = %s\n', mat2str(size(cool(0))));
