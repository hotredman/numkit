clear

import compat.*

% wextend — 1-D boundary extension.
% Modes: sym | per | zpd | ppd; sides: b (default) | l | r.

x = [1 2 3 4 5];

fprintf('=== sym (whole-point symmetric) ===\n');
disp(wextend(1, 'sym', x, 2));
fprintf('  expect: [2 1 1 2 3 4 5 5 4]\n\n');

fprintf('=== per (periodic, odd N pre-pads x(end)) ===\n');
disp(wextend(1, 'per', x, 2));
fprintf('  expect: [5 5 1 2 3 4 5 5 1 2]\n\n');

fprintf('=== zpd (zero pad) ===\n');
disp(wextend(1, 'zpd', x, 2));
fprintf('  expect: [0 0 1 2 3 4 5 0 0]\n\n');

fprintf('=== ppd (true periodic) ===\n');
disp(wextend(1, 'ppd', x, 2));
fprintf('  expect: [4 5 1 2 3 4 5 1 2]\n\n');

fprintf('=== single-side ===\n');
disp(wextend(1, 'sym', x, 2, 'l'));
fprintf('  expect: [2 1 1 2 3 4 5]\n');
disp(wextend(1, 'sym', x, 2, 'r'));
fprintf('  expect: [1 2 3 4 5 5 4]\n');
