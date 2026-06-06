% Stem Plot — Discrete signal
% Visualize a sampled sinusoid as vertical lines with markers.
% Stem is the standard way to display discrete-time signals.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

n = 0:20;
y = sin(2 * pi * n / 10);
figure
stem(n, y)
title('Discrete Sinusoid')
xlabel('Sample n')
ylabel('Amplitude')
grid on
