clear

import compat.*

% lines — MATLAB's default colororder cycle.

fprintf('--- size(lines()) ---\n');
L = lines();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(L)));

fprintf('\n--- lines(7) (full single cycle) ---\n');
disp(lines(7));
fprintf('  expect MATLAB R2025b default 7-row palette\n');

fprintf('\n--- lines(1) ---\n');
disp(lines(1));
fprintf('  expect [0 0 1]\n');

fprintf('\n--- lines(0) size ---\n');
fprintf('size lines(0)  = %s\n', mat2str(size(lines(0))));
