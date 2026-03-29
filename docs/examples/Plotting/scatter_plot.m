% Scatter Plot — Random scatter
% Generate random 2D points and display as a scatter plot.
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
