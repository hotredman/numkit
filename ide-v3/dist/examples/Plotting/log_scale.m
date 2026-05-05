% Log Scale — Logarithmic axes
% Demonstrates semilogy for exponential data and loglog for
% power-law relationships.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

% Exponential growth on semilogy
x = 1:10;
y = 2 .^ x;

figure(1)
semilogy(x, y)
title('Exponential Growth (semilogy)')
xlabel('x')
ylabel('2^x')
grid on

% Power law on loglog
x2 = logspace(0, 3, 50);
y2 = x2 .^ 2.5;

figure(2)
loglog(x2, y2)
title('Power Law x^{2.5} (loglog)')
xlabel('x')
ylabel('y')
grid on
