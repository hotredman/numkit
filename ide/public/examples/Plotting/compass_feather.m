% compass + feather — Vector arrows
% compass(U, V)  — arrows from the origin to (U_i, V_i)
% feather(U, V)  — arrows from points (i, 0) on the x-axis to
%                  (i + U_i, V_i)
% Both are quiver wrappers with deterministic seeds.
import compat.*;

clear
close all

% Equally-spaced unit vectors around a circle.
n = 12;
theta = linspace(0, 2*pi, n + 1);
theta = theta(1:n);
U = cos(theta);
V = sin(theta);

figure
subplot(1, 2, 1)
compass(U, V)
title('compass — radial vectors')

subplot(1, 2, 2)
feather(U, V)
title('feather — same vectors on the X-axis')
