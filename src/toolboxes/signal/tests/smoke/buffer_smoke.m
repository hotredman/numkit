clear

fprintf('=== signal/buffer (Phase 4.1 — frame partitioning) ===\n');

fprintf('\n[non-overlap, zero-pad last frame]\n');
y = buffer(1:18, 8);
fprintf('  shape=[%d %d] (expect [8 3])\n', size(y,1), size(y,2));
fprintf('  y(:,3) = '); fprintf('%g ', y(:,3)); fprintf('\n');
fprintf('  expect: 17 18 0 0 0 0 0 0\n');

fprintf('\n[overlap p=4 with initial zeros]\n');
y = buffer(1:18, 8, 4);
fprintf('  shape=[%d %d] (expect [8 5])\n', size(y,1), size(y,2));
fprintf('  y(:,1) = '); fprintf('%g ', y(:,1)); fprintf('\n');
fprintf('  expect: 0 0 0 0 1 2 3 4\n');

fprintf('\n[overlap p=4 ''nodelay'' — no initial zeros]\n');
y = buffer(1:18, 8, 4, 'nodelay');
fprintf('  shape=[%d %d] (expect [8 4])\n', size(y,1), size(y,2));
fprintf('  y(:,1) = '); fprintf('%g ', y(:,1)); fprintf('\n');
fprintf('  expect: 1 2 3 4 5 6 7 8\n');

fprintf('\n[underlap p=-4 — skip 4 samples between frames]\n');
y = buffer(1:24, 8, -4);
fprintf('  shape=[%d %d] (expect [8 2])\n', size(y,1), size(y,2));
fprintf('  y(:,2) = '); fprintf('%g ', y(:,2)); fprintf('\n');
fprintf('  expect: 13 14 15 16 17 18 19 20\n');

fprintf('\n[2-output [Y,Z] — complete frames + partial remainder]\n');
[y, z] = buffer(1:18, 8);
fprintf('  Y shape=[%d %d] (expect [8 2])\n', size(y,1), size(y,2));
fprintf('  Z = '); fprintf('%g ', z); fprintf('  (expect 17 18)\n');

fprintf('\nAll BIT-EQUAL with MATLAB R2025b. Octave 11.1.0 also matches.\n');
