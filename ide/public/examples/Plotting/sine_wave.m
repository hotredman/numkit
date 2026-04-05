% Sine Wave — Basic line plot
% Plot a smooth sine curve with axis labels and title.
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
