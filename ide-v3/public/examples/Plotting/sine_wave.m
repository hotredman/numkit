% Sine Wave — Basic line plot
% Plot a smooth sine curve with axis labels and title.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

x = linspace(0, 2*pi, 100);
y = sin(x);
figure
plot(x, y)
title('Sine Wave')
xlabel('x')
ylabel('sin(x)')
grid on
