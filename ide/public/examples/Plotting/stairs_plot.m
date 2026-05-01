% Stairs Plot — Staircase function
% Displays data as a step function, typical for digital signals
% or zero-order hold representations.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

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
