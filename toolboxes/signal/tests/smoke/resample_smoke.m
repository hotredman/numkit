clear

import compat.*

% randsample without replacement
rng(42);
fprintf('--- randsample(10, 5) (no replacement) ---\n');
s = randsample(10, 5);
disp(s');
fprintf('  expect: 5 distinct values from 1..10\n\n');

% randsample with replacement
fprintf('--- randsample(5, 10, true) (with replacement) ---\n');
s = randsample(5, 10, true);
disp(s');
fprintf('  expect: 10 values from 1..5 with repeats possible\n\n');

% datasample on rows
X = [1 2; 3 4; 5 6; 7 8; 9 10];
fprintf('--- datasample(X, 3) ---\n');
disp(datasample(X, 3));
fprintf('  expect: 3 rows sampled from X\n\n');

% combnk
fprintf('--- combnk(4, 2) ---\n');
disp(combnk(4, 2));
fprintf('  expect: 6 rows of 2-combinations from 1..4\n');
fprintf('          = [1 2; 1 3; 1 4; 2 3; 2 4; 3 4]\n\n');

% combnk with vector input
fprintf('--- combnk([10 20 30], 2) ---\n');
disp(combnk([10 20 30], 2));
fprintf('  expect: [10 20; 10 30; 20 30]\n');
