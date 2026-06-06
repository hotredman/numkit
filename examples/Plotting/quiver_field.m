% quiver — 2-D vector field
% Arrows from (x_i, y_i) in direction (u_i, v_i). The 4th + 5th
% positional args are the components; the optional 5th positional
% scalar is the arrow scale.
import compat.*;

clear
close all

% Sample a saddle flow on a 7×7 grid (manual flatten — meshgrid is
% on the BACKLOG).
n = 7;
xs = linspace(-1, 1, n);
ys = linspace(-1, 1, n);
x = []; y = []; u = []; v = [];
for i = 1:n
    for j = 1:n
        x = [x, xs(j)];
        y = [y, ys(i)];
        % Saddle: u = x, v = -y → "X-shaped" flow lines.
        u = [u,  xs(j)];
        v = [v, -ys(i)];
    end
end

figure
quiver(x, y, u, v, 0.5)
title('Saddle Flow')
xlabel('X')
ylabel('Y')
axis('equal')
grid on
