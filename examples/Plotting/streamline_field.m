% streamslice — Streamlines through a 2-D vector field
% streamslice picks a 5×5 seed grid automatically and integrates each
% seed through the (U, V) field via 4-th-order Runge-Kutta. Lines
% stop at the grid boundary or where the flow stalls.
import compat.*;

clear
close all

% Sink + rotation: U(x,y) = -y - 0.2x ; V(x,y) = x - 0.2y
n = 11;
xs = linspace(-1, 1, n);
ys = linspace(-1, 1, n);
U = zeros(n, n);
V = zeros(n, n);
for i = 1:n
    for j = 1:n
        U(i, j) = -ys(i) - 0.2 * xs(j);
        V(i, j) =  xs(j) - 0.2 * ys(i);
    end
end

figure
streamslice(xs, ys, U, V)
title('streamslice — Spiral Sink')
xlabel('X')
ylabel('Y')
axis('equal')
grid on
