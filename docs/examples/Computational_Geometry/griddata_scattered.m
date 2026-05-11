% griddata — Interpolate Scattered Data on a Regular Grid
%
% Linear barycentric interpolation via Delaunay triangulation.
% Query points outside the convex hull get NaN.

import compat.*;

clear
close all

% Scattered samples of a smooth function.
rng(3);
N = 25;
x = rand(1, N) * 4 - 2;
y = rand(1, N) * 4 - 2;
v = sin(x) .* cos(y);    % ground truth

% Regular grid of query points.
[Xq, Yq] = meshgrid(linspace(-2, 2, 40));
Vq = griddata(x, y, v, Xq(:), Yq(:));

% Plot true sampled values vs. interpolated grid.
figure;
plot(x, y, 'r.', 'MarkerSize', 8);
hold on;
% Show contour of interpolated surface.
Vq_grid = reshape(Vq, 40, 40);
contour(Xq, Yq, Vq_grid, 12);
title('griddata — sin(x)·cos(y) reconstructed via barycentric');
xlabel('x'); ylabel('y');

% RMS error vs. ground truth on the grid (where defined).
truth = sin(Xq) .* cos(Yq);
diff = Vq_grid - truth;
err = sqrt(mean(diff(isfinite(diff)).^2));
fprintf('RMS error vs. truth (interior): %.4f\n', err);
fprintf('Hull-exterior queries: %d of %d (NaN)\n', ...
        sum(isnan(Vq(:))), numel(Vq));
