clear

import compat.*

% --- zscore on a vector: mean → 0, std → 1 (population stdev) ---
x = [1 2 3 4 5];
z = zscore(x);
fprintf('--- zscore([1..5]) ---\n');
fprintf('  z = '); disp(z);
fprintf('  mean(z) = %.6f (expect 0)\n', mean(z));
% Population stdev: sqrt(mean((x-mean(x))^2)) = sqrt(2)
fprintf('  z(end) = %.6f (expect 2/sqrt(2) = sqrt(2) = 1.4142)\n\n', z(end));

% --- normalize default == zscore ---
zn = normalize([1 2 3 4 5]);
fprintf('  normalize == zscore: max diff = %.6e (expect 0)\n\n', ...
    max(abs(z - zn)));

% --- normalize 'range' on vector → [0, 1] ---
r = normalize([1 2 3 4 5], 'range');
fprintf('--- normalize([1..5], ''range'') ---\n');
fprintf('  r = '); disp(r);
fprintf('  expect [0 0.25 0.5 0.75 1]\n\n');

% --- normalize 'center' subtracts mean ---
c = normalize([1 2 3 4 5], 'center');
fprintf('--- normalize ''center'' ---\n');
fprintf('  c = '); disp(c);
fprintf('  expect [-2 -1 0 1 2]\n\n');

% --- normalize 'norm' produces unit ℓ² ---
n = normalize([3 4], 'norm');
fprintf('--- normalize([3 4], ''norm'') ---\n');
fprintf('  n = '); disp(n);
fprintf('  expect [0.6 0.8] = [3/5, 4/5]; ||n|| = %.6f (expect 1)\n\n', ...
    sqrt(sum(n.^2)));

% --- normalize 'medianiqr' robust ---
m = normalize([1 2 3 4 5], 'medianiqr');
% median = 3, iqr = q(0.75) - q(0.25) = 4 - 2 = 2 → m = (x-3)/2
fprintf('--- normalize ''medianiqr'' ---\n');
fprintf('  m = '); disp(m);
fprintf('  expect [-1 -0.5 0 0.5 1]\n\n');

% --- rescale to [-1, 1] ---
y = rescale([1 2 3 4 5], -1, 1);
fprintf('--- rescale([1..5], -1, 1) ---\n');
fprintf('  y = '); disp(y);
fprintf('  expect [-1 -0.5 0 0.5 1]\n\n');

% --- Matrix: zscore per column ---
M = [1 10; 2 20; 3 30];
% Column 1: mean=2, std=sqrt(2/3); z = (x-2)/sqrt(2/3)
% Column 2: mean=20, std=10*sqrt(2/3); z = (x-20)/(10*sqrt(2/3))
% Both columns z should be identical (linear scaling, same shape).
zM = zscore(M);
fprintf('--- zscore(matrix) per-column ---\n');
fprintf('  column-1 z = '); disp(zM(:,1)');
fprintf('  column-2 z = '); disp(zM(:,2)');
fprintf('  expect identical columns (both linear ramps with same shape)\n');
fprintf('  max|col1 - col2| = %.6e\n', ...
    max(abs(zM(:,1) - zM(:,2))));

% --- Constant vector: rescale → all lo (degenerate) ---
deg = rescale([5 5 5], 0, 1);
fprintf('\n--- rescale on constant ---\n');
fprintf('  deg = '); disp(deg);
fprintf('  expect [0 0 0] (all-lo, MATLAB-spec)\n');
