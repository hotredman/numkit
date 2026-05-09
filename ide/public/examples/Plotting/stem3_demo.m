% stem3 — 3-D stems with tip markers
% For each (x, y, z) sample stem3 draws a vertical line from the
% z=0 plane up to z, plus a marker at the tip. Good for sparse 3-D
% sampled data where a surface would be misleading.
import compat.*;

clear
close all

% 4×4 grid of points with height = sin(x) + cos(y).
n = 4;
xs = linspace(0, 2*pi, n);
ys = linspace(0, 2*pi, n);
x = []; y = []; z = [];
for i = 1:n
    for j = 1:n
        x = [x, xs(j)];
        y = [y, ys(i)];
        z = [z, sin(xs(j)) + cos(ys(i))];
    end
end

figure
stem3(x, y, z)
title('stem3 — Sampled sin(x) + cos(y)')
xlabel('X')
ylabel('Y')
zlabel('Z')
grid on
