clear

import compat.*

% summer — green-to-yellow colormap.

fprintf('--- size(summer()) ---\n');
s = summer();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(s)));

fprintf('\n--- summer(5) ---\n');
disp(summer(5));
fprintf('  expect: r=0..1 step 0.25, g=0.5+r/2, b=0.4\n');

fprintf('\n--- summer(1) ---\n');
disp(summer(1));
fprintf('  expect [0 0.5 0.4]\n');

fprintf('\n--- summer(0) size ---\n');
fprintf('size summer(0)  = %s\n', mat2str(size(summer(0))));
