clear

import compat.*

% Two well-separated clusters around (0,0) and (10,10)
rng(42);
n = 30;
X = [randn(n, 2); randn(n, 2) + 10];   % 60×2

[idx, C, sumd] = kmeans(X, 2);
fprintf('--- kmeans(X, 2) on bimodal Gaussian ---\n');
fprintf('idx counts: %d in cluster 1, %d in cluster 2\n', ...
    sum(idx == 1), sum(idx == 2));
fprintf('  expect: 30 / 30 split (or close)\n\n');

fprintf('Centroids:\n'); disp(C);
fprintf('  expect: rows ≈ [0 0] and [10 10] (in some order)\n\n');

fprintf('Within-cluster sum of squared distances:\n'); disp(sumd);
fprintf('  expect: roughly 60 each (≈ n*(σ²·D))\n\n');

% Verify ALL cluster-1 points belong to cluster of (0,0)-ish centre.
% Find which centroid is which.
[~, idx_zero] = min(sum(C.^2, 2));   % closer to origin
mask_first_30 = idx(1:n);
correct1 = sum(mask_first_30 == idx_zero);
correct2 = sum(idx(n+1:end) ~= idx_zero);
fprintf('First 30 points classified correctly: %d / %d\n', correct1, n);
fprintf('Last 30 points classified correctly:  %d / %d\n', correct2, n);
fprintf('  expect: ≥ 28/30 each (occasional misclassifications at the noise edge)\n\n');

% Single-cluster degenerate case
[idx2, C2, sumd2] = kmeans([1 2; 3 4; 5 6], 1);
fprintf('--- kmeans single-cluster ---\n');
fprintf('idx = '); disp(idx2');
fprintf('C   = '); disp(C2);
fprintf('sumd= %.4f\n', sumd2);
fprintf('  expect: idx=[1;1;1], C=[3 4], sumd=8\n\n');

% --- 2026-05-08 audit ТЗ closure: 4-output + N-V parsing ---
Xs = [0 0; 0.1 0; 0 0.1; 0.1 0.1; ...
      5 5; 5.1 5; 5 5.1; 5.1 5.1; ...
      10 0; 10.1 0; 10 0.1; 10.1 0.1];

fprintf('--- 4-output [idx, C, sumd, D] (D = N×K squared distances) ---\n');
[idx3, ~, ~, D] = kmeans(Xs, 3, 'Replicates', 5);
fprintf('idx = '); disp(idx3');
fprintf('size(D) = [%d %d] (expect [12 3])\n\n', size(D, 1), size(D, 2));

fprintf('--- mixed-case N-V + Display + EmptyAction silent ---\n');
idx4 = kmeans(Xs, 3, 'maxiter', 200, 'Display', 'off', ...
              'EmptyAction', 'singleton');
disp(idx4');

fprintf('--- explicit Distance=cityblock — clear error ---\n');
try
    kmeans(Xs, 3, 'Distance', 'cityblock');
    fprintf('UNEXPECTED: no error\n');
catch e
    fprintf('OK: %s\n', e.message);
end
