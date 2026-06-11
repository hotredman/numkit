clear

import compat.*

% bwselect — keep CC that contain seed pixels.

fprintf('--- 5x5 with two components ---\n');
A = logical([0 1 0 0 1;
             1 0 1 0 0;
             1 0 1 1 0;
             1 1 1 0 0;
             1 0 0 1 0]);
disp(A);

fprintf('--- bwselect(A, 1, 3, 4) — pick row=3 col=1 with 4-conn ---\n');
out = bwselect(A, 1, 3, 4);
disp(out);
fprintf('  expect: left-column blob (pixels at col=1, rows 2..5)\n\n');

fprintf('--- bwselect(A, 1, 3, 8) — same seed with 8-conn ---\n');
out = bwselect(A, 1, 3, 8);
disp(out);
fprintf('  expect: bigger blob (8-conn merges left col + col=3 patch)\n\n');

fprintf('--- multi-seed [3 4], [3 5] (col=3,4; row=3,5) ---\n');
out = bwselect(A, [3 4], [3 5], 4);
disp(out);

fprintf('--- two-output: idx returned ---\n');
[mask, idx] = bwselect(A, 1, 3, 4);
fprintf('idx (1-based linear, col-major):\n');
disp(idx');

fprintf('--- empty seeds → all-zero mask ---\n');
out = bwselect(A, [], [], 8);
fprintf('any true? %d\n', any(out(:)));
