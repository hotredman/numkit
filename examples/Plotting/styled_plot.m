% Styled Plot — Line styles, markers, and name-value pairs
% MATLAB style strings: color (r,g,b,...), line (-,--,:,-.)
% and markers (o,s,d,^,*,+,x,.).
% Name-value pairs: 'LineWidth', 'MarkerSize'.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

x = linspace(0, 2*pi, 30);

figure
hold on
plot(x, sin(x), 'b-', 'LineWidth', 2)
plot(x, sin(x - pi/4), 'r--o', 'LineWidth', 1.5, 'MarkerSize', 4)
plot(x, sin(x - pi/2), 'g:s', 'MarkerSize', 5)
hold off
title('Style Strings & Name-Value Pairs')
legend('solid blue', 'dashed red + circles', 'dotted green + squares')
grid on
