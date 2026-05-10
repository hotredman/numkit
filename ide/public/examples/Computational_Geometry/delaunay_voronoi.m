% delaunay + voronoi — Triangulation and its Dual
%
% delaunay(x, y) returns triangle indices (M×3) — each row is a
% Delaunay triangle satisfying the empty-circumcircle property.
% voronoi(x, y) draws the dual Voronoi diagram (edges between
% circumcenters of adjacent Delaunay triangles).

import compat.*;

clear
close all

% Scattered points.
rng(7);
N = 15;
x = rand(1, N) * 4;
y = rand(1, N) * 4;

% Delaunay triangulation — emit triangles as lines for visualization.
tri = delaunay(x, y);
figure;
hold on;
for i = 1:size(tri, 1)
    j = tri(i, :);
    plot([x(j(1)) x(j(2)) x(j(3)) x(j(1))], ...
         [y(j(1)) y(j(2)) y(j(3)) y(j(1))], ...
         'b-', 'LineWidth', 0.8);
end
plot(x, y, 'r.', 'MarkerSize', 8);
title(sprintf('Delaunay triangulation — %d triangles over %d points', ...
              size(tri, 1), N));
xlabel('x'); ylabel('y');

% Voronoi diagram — automatic via dual.
figure;
voronoi(x, y);
title(sprintf('Voronoi diagram — same %d points', N));
xlabel('x'); ylabel('y');

fprintf('delaunay/voronoi demo: %d triangles, dual Voronoi\n', ...
        size(tri, 1));
