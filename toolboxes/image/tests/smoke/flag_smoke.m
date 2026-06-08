clear

import compat.*

% flag — cyclic red/white/blue/black colormap.

fprintf('--- size(flag()) ---\n');
f = flag();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(f)));

fprintf('\n--- flag(8) cycles ---\n');
disp(flag(8));
fprintf('  expect cycle [1 0 0; 1 1 1; 0 0 1; 0 0 0] twice\n');

fprintf('\n--- flag(1) ---\n');
disp(flag(1));
fprintf('  expect [1 0 0]\n');

fprintf('\n--- flag(0) size ---\n');
fprintf('size flag(0)  = %s\n', mat2str(size(flag(0))));
