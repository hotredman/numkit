% contour + contourf — topographic-style level lines
% contour(Z) draws contour lines at 10 auto-spaced levels.
% contour(Z, n) overrides the count, contour(Z, levels) the values.
% contourf renders filled bands instead of lines (currently lines —
% filled is on the BACKLOG).
import compat.*;

clear
close all

% Synthetic terrain: smooth multi-bump landscape.
n = 40;
x = linspace(-3, 3, n);
y = linspace(-3, 3, n);
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        Z(i, j) = 3 * exp(-((x(j)-1)^2 + (y(i)-1)^2)) ...
                + 2 * exp(-((x(j)+1)^2 + (y(i)+1)^2) / 2) ...
                + 0.5 * x(j) - 0.3 * y(i);
    end
end

figure
contour(x, y, Z, 12)
title('contour — Synthetic Terrain (12 levels)')
xlabel('X')
ylabel('Y')
grid on
