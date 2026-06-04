% Dense Polar Spiral — 100,000-point polar line on the GPU
% A tight Archimedean spiral sampled at 100k points. polarplot routes the
% dense line to the WebGL canvas (one LINE_STRIP) layered over the SVG polar
% grid, so it draws and pans smoothly.
import compat.*;

clear
close all

n     = 100000;
theta = linspace(0, 60 * pi, n);
rho   = theta / (60 * pi);

figure
polarplot(theta, rho)
title('100k-point polar spiral — GPU line')
