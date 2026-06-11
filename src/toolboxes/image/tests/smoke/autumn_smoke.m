clear

import compat.*

% autumn — red-to-yellow colormap.

fprintf('--- size(autumn()) ---\n');
a = autumn();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(a)));

fprintf('\n--- autumn(5) ---\n');
disp(autumn(5));
fprintf('  expect: r=1, g=0..1 step 0.25, b=0\n');

fprintf('\n--- autumn(1) ---\n');
disp(autumn(1));
fprintf('  expect [1 0 0]\n');

fprintf('\n--- autumn(0) size ---\n');
fprintf('size autumn(0)  = %s\n', mat2str(size(autumn(0))));
