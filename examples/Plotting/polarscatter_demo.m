% polarscatter + polarhistogram — polar coordinates
% polarscatter(theta, rho) places markers in polar; polarhistogram
% bins theta values into angular sectors over [0, 2π).
import compat.*;

clear
close all

% Rose-of-the-winds: angle bins concentrated near a few preferred
% directions.
theta = [0.1 0.2 0.3 1.5 1.6 1.7 1.8 3.0 3.1 3.2 5.0 5.1];

figure
subplot(1, 2, 1)
% Markers along a logarithmic spiral.
t = linspace(0, 4*pi, 30);
rho = exp(t / 8);
polarscatter(t, rho)
title('polarscatter — Log Spiral')

subplot(1, 2, 2)
polarhistogram(theta, 12)
title('polarhistogram — 12 bins')
