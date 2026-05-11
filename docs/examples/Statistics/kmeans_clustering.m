% kmeans_clustering.m
% k-means clustering on a synthetic 3-cluster 2-D dataset. Shows the
% recovered cluster assignments + centroid locations vs ground truth.

import compat.*;

clear
close all

rng(0);

% Three Gaussian blobs.
n = 80;
C1 = bsxfun(@plus, [0  0],  0.5 * randn(n, 2));
C2 = bsxfun(@plus, [3  3],  0.6 * randn(n, 2));
C3 = bsxfun(@plus, [3 -2],  0.5 * randn(n, 2));
X  = [C1; C2; C3];
truth = [ones(n,1); 2*ones(n,1); 3*ones(n,1)];

K = 3;
[idx, C] = kmeans(X, K);

fprintf('k-means on %d points, K=%d\n', size(X, 1), K);
fprintf('Centroids found:\n');
disp(C);

% Plot.
figure;
subplot(1, 2, 1);
gscatter(X(:,1), X(:,2), truth);
title('Ground truth labels');
axis equal; grid on;

subplot(1, 2, 2);
gscatter(X(:,1), X(:,2), idx);
hold on;
plot(C(:,1), C(:,2), 'k*', 'MarkerSize', 14, 'LineWidth', 2);
hold off;
title('k-means assignments + centroids');
axis equal; grid on;
sgtitle('k-means clustering');
