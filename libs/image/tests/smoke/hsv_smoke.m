clear

import compat.*

% hsv — hue-rotation colormap.

fprintf('--- size(hsv()) ---\n');
h = hsv();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(h)));

fprintf('\n--- hsv(6) primary 6-step ---\n');
disp(hsv(6));
fprintf('  expect [1 0 0; 1 1 0; 0 1 0; 0 1 1; 0 0 1; 1 0 1]\n');

fprintf('\n--- hsv(1) ---\n');
disp(hsv(1));
fprintf('  expect [1 0 0]\n');

fprintf('\n--- hsv(0) size ---\n');
fprintf('size hsv(0)  = %s\n', mat2str(size(hsv(0))));
