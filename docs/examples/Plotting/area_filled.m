% area — Filled curve under the line
% area(x, y) shades the region between the curve and y=0.
% area(x, y, base) lifts the baseline to `base`.
import compat.*;

clear
close all

x = linspace(0, 2*pi, 80);
y = sin(x) + 0.5 * sin(3*x);

figure
area(x, y)
title('area — sin(x) + 0.5 sin(3x)')
xlabel('x')
ylabel('y')
grid on
