clear

import compat.*

% winter — blue→cyan-ish colormap.

fprintf('--- size(winter()) ---\n');
w = winter();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(w)));

fprintf('\n--- winter(5) ---\n');
disp(winter(5));
fprintf('  expect: r=0, g=0..1 step 0.25, b=1-g/2\n');

fprintf('\n--- winter(1) ---\n');
disp(winter(1));
fprintf('  expect [0 0 1]\n');

fprintf('\n--- winter(0) size ---\n');
fprintf('size winter(0)  = %s\n', mat2str(size(winter(0))));
