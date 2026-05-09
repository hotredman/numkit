% mesh — Wireframe surface
% mesh(Z) draws the same data as surf but as a line lattice instead
% of a filled mesh. Cheaper to read for noisy data where filled faces
% would obscure the structure.
import compat.*;

clear
close all

n = 20;
x = linspace(-3, 3, n);
y = linspace(-3, 3, n);
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        Z(i, j) = sin(x(j)) * cos(y(i));
    end
end

figure
mesh(x, y, Z)
title('Wireframe of sin(x) cos(y)')
xlabel('X')
ylabel('Y')
zlabel('Z')
grid on
