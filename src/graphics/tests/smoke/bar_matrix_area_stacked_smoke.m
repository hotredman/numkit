clear;

% bar(matrix) + area(matrix) — stacked / grouped multi-series.

Y = [1 2 3; 4 5 6; 7 8 9];

% bar — grouped (default)
bar(Y);
fprintf('bar(Y) grouped OK — 3 datasets, sub-pixel offset\n');

% bar — stacked
figure;
bar(Y, 'stacked');
fprintf('bar(Y, stacked) OK — cumulative-sum bars\n');

% area — stacked
figure;
area(Y);
fprintf('area(Y) stacked OK — 3 cumulative-sum areas\n');

% bar — single vector back-compat
figure;
bar([1 2 3 4 5]);
fprintf('bar(vector) single-series OK\n');

fprintf('bar / area matrix smoke DONE\n');
