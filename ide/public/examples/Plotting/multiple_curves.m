% Multiple Curves — Plot sin & cos together
% Use hold on to overlay two curves on the same axes.
clear
close all

x = linspace(0, 2*pi, 100);
figure
hold on
plot(x, sin(x))
plot(x, cos(x))
hold off
title('Trig Functions')
legend('sin(x)', 'cos(x)')
grid on
