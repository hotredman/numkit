% convhull — Convex Hull of a Point Cloud
%
% Andrew's monotone-chain algorithm. Returns 1-based indices of
% hull vertices in CCW order, with the first vertex repeated at the
% end (MATLAB convention). Use it to draw a hull around a scatter.

import compat.*;

clear
close all

% Random-ish point cloud.
N = 30;
rng(42);
x = randn(1, N);
y = randn(1, N);

% Get hull indices.
k = convhull(x, y);

% Plot points + hull polygon.
figure;
plot(x, y, 'k.', 'MarkerSize', 4);
hold on;
plot(x(k), y(k), 'r-', 'LineWidth', 2);
title(sprintf('Convex hull of %d random points (%d-vertex polygon)', ...
              N, length(k) - 1));
xlabel('x'); ylabel('y');
legend('points', 'hull');

% Hull's enclosed area via polyarea.
A = polyarea(x(k), y(k));
fprintf('Hull encloses area = %.4f\n', A);
fprintf('Number of hull vertices: %d (last repeated, so %d unique)\n', ...
        length(k), length(k) - 1);
