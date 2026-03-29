% Polar Pattern — Antenna array radiation
% 8-element linear array with uniform spacing d = lambda/2.
% Demonstrates polarplot with normalized amplitude.
clear
close all

N = 8;
d = 0.5;
theta = linspace(0, 2*pi, 720);

% Array factor
AF = zeros(1, length(theta));
for n = 0:N-1
    AF = AF + exp(1i * 2 * pi * d * n * cos(theta));
end
AF = abs(AF) / max(abs(AF));

figure
polarplot(theta, AF)
title('8-element array, d = lambda/2')
grid on
