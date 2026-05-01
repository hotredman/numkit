% Axis Modes — equal, tight, ij
% axis('equal') makes 1 data unit the same length on both axes.
% axis('tight') removes padding. axis('ij') flips Y (image coords).
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

% Circle should look round with axis equal
t = linspace(0, 2*pi, 100);

figure(1)
plot(cos(t), sin(t))
axis equal
title('Unit Circle — axis equal')
grid on

% Tight: no padding around data
figure(2)
x = linspace(0, 10, 50);
plot(x, sin(x) .* exp(-x/5))
axis tight
title('Damped Sine — axis tight')
grid on
