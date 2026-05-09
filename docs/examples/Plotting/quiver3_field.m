% quiver3 — 3-D vector field
% Sample a flow on a 5×5×5 grid and draw an arrow at every node
% pointing in the field direction. (u, v, w) are the per-point
% velocity components; the optional last arg scales the arrow length.
import compat.*;

clear
close all

% Build a sparse grid (manual flatten — meshgrid is on the BACKLOG).
n = 5;
xs = linspace(-1, 1, n);
ys = linspace(-1, 1, n);
zs = linspace(-1, 1, n);

x = []; y = []; z = [];
u = []; v = []; w = [];
for i = 1:n
    for j = 1:n
        for k = 1:n
            x = [x, xs(j)];
            y = [y, ys(i)];
            z = [z, zs(k)];
            % Curl-like flow: (-y, x, z/2)
            u = [u, -ys(i)];
            v = [v,  xs(j)];
            w = [w,  zs(k) / 2];
        end
    end
end

figure
quiver3(x, y, z, u, v, w, 0.3)
title('quiver3 — Curl Flow Sampled at 5\times5\times5 Nodes')
xlabel('X')
ylabel('Y')
zlabel('Z')
grid on
view(45, 30)
