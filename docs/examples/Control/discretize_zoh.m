% discretize_zoh.m
% Convert a continuous-time plant to discrete via zero-order hold
% (c2d, ZOH method) at three sample rates. Show how Ts choice affects
% the discrete step response vs the continuous reference.

import compat.*;

clear
close all

Gc = tf(1, [1, 0.4, 1]);              % under-damped 2nd-order

t  = linspace(0, 20, 400);
yc = step(Gc, t);

% Three sample times.
Ts_list = [0.1, 0.5, 1.5];

figure;
plot(t, yc, 'k', 'LineWidth', 2); grid on;
hold on;
colors = 'bgr';
for k = 1:numel(Ts_list)
    Ts = Ts_list(k);
    Gd = c2d(Gc, Ts, 'zoh');
    [yd, td] = step(Gd, t(end));
    stairs(td, yd, colors(k), 'LineWidth', 1.2);
end
hold off;
xlabel('time (s)'); ylabel('output');
legend('continuous', 'Ts=0.1', 'Ts=0.5', 'Ts=1.5', 'location', 'southeast');
title('c2d with ZOH — discrete step at three sample rates');
