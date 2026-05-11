% correlation_matrix.m
% Build a correlated multivariate dataset, compute the full
% correlation matrix with corrcoef, and visualize it as a heatmap.
% Also draw the pair-scatter for the strongest positive pair.

import compat.*;

clear
close all

rng(0);
N = 200;

% Three independent baselines + a derived pair.
a = randn(N, 1);
b = 0.7 * a + 0.5 * randn(N, 1);   % strongly correlated with a
c = -0.5 * a + 0.7 * randn(N, 1);  % anti-correlated with a
d = randn(N, 1);                    % independent

X = [a, b, c, d];
labels = {'a', 'b', 'c', 'd'};

R = corrcoef(X);
fprintf('Correlation matrix:\n');
disp(R);

figure;
subplot(1, 2, 1);
imagesc(R, [-1, 1]); colorbar;
colormap('jet');
xticks(1:4); yticks(1:4);
xticklabels(labels); yticklabels(labels);
title('corrcoef(X)');
axis square;

subplot(1, 2, 2);
plot(a, b, 'bo', 'MarkerSize', 5); grid on;
xlabel('a'); ylabel('b');
title(sprintf('Strongest positive pair: r(a,b) = %.3f', R(1, 2)));
sgtitle('Correlation matrix + pair scatter');
