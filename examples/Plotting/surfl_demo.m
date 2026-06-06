% surfl — Surface with automatic lighting
% surfl(Z) draws the surface AND auto-attaches a headlight + sets
% lighting='gouraud' + material='shiny' in one call. Equivalent to
% the surf + camlight combo but a single line.
import compat.*;

clear
close all

n = 25;
x = linspace(-2, 2, n);
y = linspace(-2, 2, n);
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        Z(i, j) = exp(-(x(j)^2 + y(i)^2) / 2);
    end
end

figure
surfl(Z)
title('surfl — Auto-Lit Gaussian')
xlabel('X')
ylabel('Y')
zlabel('Z')
grid on
