clear

import compat.*

% gray — N×3 grayscale colormap (Octave compat).

fprintf('--- size(gray()) ---\n');
g = gray();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(g)));
fprintf('first row: %s\n', mat2str(g(1,:)));
fprintf('last row:  %s\n', mat2str(g(end,:)));

fprintf('\n--- gray(11) ---\n');
g = gray(11);
disp(g);
fprintf('  expect col 1 = [0:0.1:1]\n');

fprintf('\n--- gray(1) ---\n');
disp(gray(1));
fprintf('  expect [0 0 0]\n');

fprintf('\n--- gray(0), gray(-1) ---\n');
fprintf('size gray(0)  = %s\n', mat2str(size(gray(0))));
fprintf('size gray(-1) = %s\n', mat2str(size(gray(-1))));
