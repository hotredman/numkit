clear

% wcodemat — quantize/scale matrix into integer codes [1, NB].

X = [-2 -1 0 1 2];
fprintf('--- wcodemat([-2..2], 5, "mat", 1) — abs scaled to [1,5] ---\n');
disp(wcodemat(X, 5, 'mat', 1));
fprintf('  expect: abs values |X|=[2 1 0 1 2] → [5 3 1 3 5] (scaled)\n\n');

fprintf('--- absol=0: signed scaling ---\n');
disp(wcodemat(X, 5, 'mat', 0));
fprintf('  expect: [-2..2] → [1..5]\n\n');

fprintf('--- per-row ---\n');
M = [-1 0 1; -2 0 2];
disp(wcodemat(M, 5, 'row', 1));
fprintf('  each row scaled separately to [1,5]\n\n');

fprintf('--- default (NB=16, mat, abs) ---\n');
disp(wcodemat([0 0.5 1 -0.25]));
