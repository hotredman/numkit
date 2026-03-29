% Stairs Plot — Staircase function
% Displays data as a step function, typical for digital signals
% or zero-order hold representations.
clear
close all

t = 0:0.5:5;
y = round(3 * sin(t));
figure
stairs(t, y)
title('Quantized Sine (Zero-Order Hold)')
xlabel('Time')
ylabel('Level')
grid on
