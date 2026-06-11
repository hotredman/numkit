clear

import compat.*

% freqspace — 2-D frequency-domain coordinate vectors.

fprintf('--- single-output, even N=8 ---\n');
disp(freqspace(8));
fprintf('  expect [0 0.25 0.5 0.75 1]\n\n');

fprintf('--- single-output, odd N=5 ---\n');
disp(freqspace(5));
fprintf('  expect [0 0.4 0.8]\n\n');

fprintf('--- whole form, N=7 ---\n');
disp(freqspace(7, 'whole'));
fprintf('  expect 7-vec [0, 2/7, 4/7, ..., 12/7]\n\n');

fprintf('--- 2-output, N=8 (centered, even) ---\n');
[f1, f2] = freqspace(8);
fprintf('f1: %s\n', mat2str(f1, 4));
fprintf('f2: %s\n', mat2str(f2, 4));
fprintf('  expect [-1 -0.75 -0.5 -0.25 0 0.25 0.5 0.75]\n\n');

fprintf('--- 2-output, [4 6] dims ---\n');
[f1, f2] = freqspace([4 6]);
fprintf('f1 (cols=6): %s\n', mat2str(f1, 4));
fprintf('f2 (rows=4): %s\n', mat2str(f2, 4));
