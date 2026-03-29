% Axis Modes — equal, tight, ij
% axis('equal')  makes units equal on both axes (circles look round).
% axis('tight')  removes padding around data.
% axis('ij')     flips Y axis (origin top-left, like image coordinates).
clear
close all

% Draw a unit circle — needs axis equal to look round
t = linspace(0, 2*pi, 100);

figure(1)
plot(cos(t), sin(t))
axis equal
title('Unit Circle — axis equal')
grid on

% Tight axis — no extra padding
figure(2)
x = linspace(0, 10, 50);
plot(x, sin(x) .* exp(-x/5))
axis tight
title('Damped Sine — axis tight')
grid on
