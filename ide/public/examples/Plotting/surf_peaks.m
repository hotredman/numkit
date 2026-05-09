% surf Peaks — Face-shaded 3-D surface
% Build a "peaks-like" surface from a sum of Gaussians and render
% it through the WebGL surf path: indexed triangle mesh + per-vertex
% colors sampled from a height-driven colormap + Lambert shading.
import compat.*;

clear
close all

n = 40;
x = linspace(-3, 3, n);
y = linspace(-3, 3, n);
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        Z(i, j) = 3 * (1 - x(j))^2 * exp(-(x(j)^2) - (y(i) + 1)^2) ...
                - 10 * (x(j)/5 - x(j)^3 - y(i)^5) * exp(-x(j)^2 - y(i)^2) ...
                - (1/3) * exp(-(x(j)+1)^2 - y(i)^2);
    end
end

figure
surf(x, y, Z)
title('Peaks Surface')
xlabel('X')
ylabel('Y')
zlabel('Z')
grid on
view(-37.5, 30)
