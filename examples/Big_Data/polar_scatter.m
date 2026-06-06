% Big Polar Scatter — 50,000 points on the GPU
% polarscatter with 50k (theta, rho) samples spread uniformly over the disc.
% The polar grid stays SVG; the points draw on a WebGL canvas that lands
% exactly on the grid — a dense polar cloud that would choke as 50k SVG
% circles stays smooth.
import compat.*;

clear
close all

n     = 50000;
theta = 2 * pi * rand(1, n);
rho   = sqrt(rand(1, n));          % sqrt → uniform area density

figure
polarscatter(theta, rho)
title('50k-point polar scatter — GPU points')
