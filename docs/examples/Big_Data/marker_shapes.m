% Marker Shapes — every GL marker on large scatter
% Eleven 8k-point clusters, each a different marker shape (o s d ^ v < > p h
% + x), all drawn on the GPU point path via the shape atlas. A big categorical
% scatter with mixed markers stays fast and the shapes match the SVG fallback.
import compat.*;

clear
close all

markers = {'o', 's', 'd', '^', 'v', '<', '>', 'p', 'h', '+', 'x'};
n = 8000;

figure
hold on
for k = 1:numel(markers)
    ang = (k - 1) / numel(markers) * 2 * pi;
    px = 6 * cos(ang) + randn(1, n) * 0.8;
    py = 6 * sin(ang) + randn(1, n) * 0.8;
    plot(px, py, markers{k})
end
hold off
title('GL marker shapes - 11 clusters x 8k points')
xlabel('x')
ylabel('y')
