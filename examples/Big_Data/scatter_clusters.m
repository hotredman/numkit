% Big Scatter — 200,000 points on the GPU
% Four Gaussian clusters, 200k markers total. The viewer routes round-marker
% scatter above ~2,000 points to a WebGL canvas (a single POINTS draw), so it
% stays interactive where 200k individual SVG markers would lock up the page.
import compat.*;

clear
close all

n  = 50000;                        % points per cluster
cx = [0 6 3 -4];
cy = [0 1 -5 4];
x = [];
y = [];
for k = 1:numel(cx)
    x = [x, cx(k) + randn(1, n)];
    y = [y, cy(k) + randn(1, n)];
end

figure
scatter(x, y)
title('200k-point scatter — GPU points')
xlabel('x')
ylabel('y')
grid on
