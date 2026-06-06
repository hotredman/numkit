% contour3 + surfc — Contour lines on a 3-D surface
% surfc draws a face-shaded surface AND a set of contour lines that
% ride at the height of the surface. Internally that's surf + contour3
% combined on the same axes.
import compat.*;

clear
close all

n = 30;
x = linspace(-2.5, 2.5, n);
y = linspace(-2.5, 2.5, n);
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        Z(i, j) = -(x(j)^2 + y(i)^2) + 3 * cos(x(j)) * sin(y(i));
    end
end

figure
surfc(x, y, Z)
title('surfc — Surface + Contours at Surface Height')
xlabel('X')
ylabel('Y')
zlabel('Z')
grid on
view(-37.5, 35)
