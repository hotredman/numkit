% Subplot Demo — Multiple plots in one figure
% subplot(rows, cols, position) creates a grid of axes.
% Each subplot has its own title, labels, and data.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

x = linspace(0, 2*pi, 100);

figure

subplot(2, 2, 1)
plot(x, sin(x))
title('sin(x)')
grid on

subplot(2, 2, 2)
plot(x, cos(x), 'r')
title('cos(x)')
grid on

subplot(2, 2, 3)
plot(x, sin(2*x), 'g')
title('sin(2x)')
grid on

subplot(2, 2, 4)
plot(x, cos(2*x), 'm')
title('cos(2x)')
grid on
