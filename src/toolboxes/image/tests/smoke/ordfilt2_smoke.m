clear

% ordfilt2 — 2-D order-statistic filter.

b = [ 0  1  2  3
      1  8 12 12
      4 20 24 21
      7 22 25 18];

% --- 9th of 9 (max) over 3x3 zeros pad — Octave reference ---
fprintf('--- ordfilt2(b, 9, true(3)) — 3x3 max ---\n');
disp(ordfilt2(b, 9, true(3)));
fprintf('  expect:\n   8 12 12 12\n  20 24 24 24\n  22 25 25 25\n  22 25 25 25\n\n');

% --- 8th of 9 over 3x3 ---
fprintf('--- ordfilt2(b, 8, true(3)) ---\n');
disp(ordfilt2(b, 8, true(3)));
fprintf('  expect:\n   1  8 12 12\n   8 20 21 21\n  20 24 24 24\n  20 24 24 24\n\n');

% --- 7th of 9 with symmetric padding ---
fprintf('--- ordfilt2(b, 7, true(3), "symmetric") ---\n');
disp(ordfilt2(b, 7, true(3), 'symmetric'));
fprintf('  expect:\n   1  2  8 12\n   4 12 20 21\n   8 22 22 21\n  20 24 24 24\n\n');

% --- 1st of 9 = min ---
fprintf('--- ordfilt2(b, 1, true(3)) — min ---\n');
disp(ordfilt2(b, 1, true(3)));
fprintf('  (min over 3x3 zero-padded)\n');

% --- 5th of 9 = median (matches medfilt2) ---
fprintf('\n--- median check: ordfilt2(b, 5, true(3)) vs medfilt2(b) ---\n');
o = ordfilt2(b, 5, true(3));
m = medfilt2(b);
fprintf('match = %d (expect 1)\n', isequal(o, m));
