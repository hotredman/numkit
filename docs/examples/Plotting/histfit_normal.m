% histfit — Histogram + Gaussian fit overlay
% Bins x into N bins and overlays a Gaussian PDF scaled to match
% the bar heights. Useful for "is this Gaussian?" eyeball checks.
import compat.*;

clear
close all

x = randn(1, 500);

figure
histfit(x, 25)
title('histfit — 500 Standard-Normal Samples + Fit')
xlabel('value')
ylabel('count')
grid on
