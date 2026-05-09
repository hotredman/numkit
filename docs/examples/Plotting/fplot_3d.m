% fplot family — Plot from anonymous function handles
% fplot(@(x) f(x), [a b]) samples f on a dense linspace.
% fcontour(@(x,y) g(x,y), [xa xb ya yb])  → 30x30 contour
% fsurf / fmesh — same idea, 30x30 surface.
%
% Function handles work on both engine backends; you can pass
% closures that capture variables from the surrounding script.
import compat.*;

clear
close all

% 1-D plot of a damped sinusoid.
figure
fplot(@(x) exp(-x/4) .* sin(2*x), [0 10])
title('fplot — exp(-x/4) sin(2x)')
xlabel('x')
ylabel('f(x)')
grid on

% 3-D surface of a separable Gaussian.
figure
fsurf(@(x, y) exp(-(x.^2 + y.^2) / 2), [-3 3 -3 3])
title('fsurf — Gaussian bell')
xlabel('x')
ylabel('y')
zlabel('z')
grid on
view(-37.5, 30)
