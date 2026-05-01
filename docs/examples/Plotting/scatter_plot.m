% Scatter Plot — Random scatter
% Generate random 2D points and display as a scatter plot.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

x = rand(1, 30);
y = rand(1, 30);
figure
scatter(x, y)
title('Random Points')
xlabel('x')
ylabel('y')
grid on
