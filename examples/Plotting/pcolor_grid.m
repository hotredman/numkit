% pcolor — Cell-vertex aligned pseudocolor plot
% Like imagesc but the (x, y) coordinates sit at cell VERTICES
% rather than centres — useful when the grid is irregular and
% you want exact spatial alignment.
import compat.*;

clear
close all

n = 30;
x = linspace(-3, 3, n);
y = linspace(-3, 3, n);
Z = zeros(n, n);
for i = 1:n
    for j = 1:n
        Z(i, j) = sin(x(j)) * cos(y(i));
    end
end

figure
pcolor(x, y, Z)
colorbar()
title('pcolor — sin(x) cos(y)')
xlabel('X')
ylabel('Y')
