clear
import compat.*

% interp2 'cubic' — Keys bicubic convolution (a = -0.5) on a uniform grid.
V = [1 2 4 8; 3 5 9 17; 6 11 20 33; 10 18 30 48];

fprintf('interior (2.5,2.5): %.10g\n', interp2(V, 2.5, 2.5, 'cubic'));  % 10.52734375
fprintf('boundary (1.3,3.7): %.10g\n', interp2(V, 1.3, 3.7, 'cubic'));  % 10.38295
fprintf('on node  (2,3)    : %.10g\n', interp2(V, 2, 3, 'cubic'));      % 11

% Smooth surface — cubic is exact at the midpoint.
[X, Y] = meshgrid(1:4, 1:4);
W = X.^2 + Y;
fprintf('X,Y,V quadratic   : %.10g\n', interp2(X, Y, W, 2.5, 2.5, 'cubic'));  % 8.75

% Linear differs (cannot capture curvature).
fprintf('linear (2.5,2.5)  : %.10g\n', interp2(V, 2.5, 2.5));  % 11.25
