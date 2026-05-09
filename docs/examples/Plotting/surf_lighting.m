% surf + Lighting — phong shading and material presets
% After surf draws the surface, lighting('phong') switches to per-pixel
% specular highlights, material('shiny') tunes the specular response,
% and camlight('headlight') attaches a moving light to the camera so
% the surface always looks lit no matter how you orbit.
import compat.*;

clear
close all

n = 30;
x = linspace(-2, 2, n);
y = linspace(-2, 2, n);
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        Z(i, j) = sin(sqrt(x(j)^2 + y(i)^2) * 2) ...
                / (sqrt(x(j)^2 + y(i)^2) + 0.5);
    end
end

figure
surf(x, y, Z)
lighting('phong')
material('shiny')
camlight('headlight')
title('Phong-Shaded Bessel-like Surface')
xlabel('X')
ylabel('Y')
zlabel('Z')
grid on
