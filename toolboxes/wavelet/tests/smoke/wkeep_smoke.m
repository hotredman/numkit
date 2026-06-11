clear

import compat.*

% wkeep — central / left / right / numeric-FIRST window extractor.

x = 1:10;
fprintf('=== central (default) ===\n');
disp(wkeep(x, 4));
fprintf('  expect: [4 5 6 7]\n\n');

fprintf('=== left / right / numeric ===\n');
disp(wkeep(x, 4, 'l'));    fprintf('  expect: [1 2 3 4]\n');
disp(wkeep(x, 4, 'r'));    fprintf('  expect: [7 8 9 10]\n');
disp(wkeep(x, 4, 3));      fprintf('  expect: [3 4 5 6]\n\n');

fprintf('=== odd N central / odd K central ===\n');
disp(wkeep(1:9, 4));       fprintf('  expect: [3 4 5 6]\n');
disp(wkeep(x, 3));         fprintf('  expect: [4 5 6]\n');
