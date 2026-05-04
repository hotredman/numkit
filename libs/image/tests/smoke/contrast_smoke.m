clear

import compat.*

% contrast — gray contrast-stretching colormap (MATLAB-compatible).

X = reshape(1:16, 4, 4);  % uniform 1..16 (same value-set as magic(4))

fprintf('--- contrast(1:16, 8) ---\n');
disp(contrast(X, 8));
fprintf('  expect [0.125; 0.25; 0.375; 0.5; 0.625; 0.75; 0.875; 1] (×3)\n\n');

fprintf('--- contrast(1:16, 4) ---\n');
disp(contrast(X, 4));
fprintf('  expect [0.2; 0.5; 0.8; 1] (×3)\n\n');

fprintf('--- contrast([10 20; 30 40], 6) ---\n');
disp(contrast([10 20; 30 40], 6));
fprintf('  expect [0.2; 0.3; 0.5; 0.7; 0.8; 1.0] (×3)\n');
