% Scatter Styles — open vs filled markers + polarscatter
% MATLAB markers are OPEN (outline in the series colour) by default;
% scatter(...,'filled') fills them. Other marker shapes come through plot's
% LineSpec (s = square, ^ = triangle, d = diamond, ...). polarscatter places
% markers in polar coordinates.
import compat.*;

clear
close all

x = linspace(0, 2*pi, 40);

figure
hold on
scatter(x, sin(x))                  % open circles (the default)
scatter(x, sin(x) - 0.7, 'filled')  % filled circles
plot(x, sin(x) - 1.4, 's')          % open squares  (markers only)
plot(x, sin(x) - 2.1, '^')          % open triangles
plot(x, sin(x) - 2.8, 'd-')         % line + open diamonds
hold off
title('Marker styles - open / filled / shapes')
legend('scatter', 'scatter filled', 'squares', 'triangles', 'line + diamonds')
grid on

figure
theta = linspace(0, 4*pi, 80);
rho   = linspace(0.1, 1, 80);
polarscatter(theta, rho)
title('polarscatter')
