% step_response.m
% Compare step responses of three classic LTI systems built with tf:
%   • first-order  G1(s) = 1 / (s + 1)
%   • second-order, well-damped     G2(s) = 4 / (s^2 + 3s + 4)
%   • second-order, under-damped    G3(s) = 4 / (s^2 + 0.6s + 4)

import compat.*;

clear
close all

G1 = tf(1, [1, 1]);
G2 = tf(4, [1, 3, 4]);
G3 = tf(4, [1, 0.6, 4]);

t = linspace(0, 12, 400);
y1 = step(G1, t);
y2 = step(G2, t);
y3 = step(G3, t);

figure;
plot(t, y1, 'b', t, y2, 'g', t, y3, 'r', 'LineWidth', 1.5);
grid on;
xlabel('time (s)'); ylabel('y(t)');
legend('1/(s+1)', '4/(s^2+3s+4)  damped', '4/(s^2+0.6s+4)  oscillatory', ...
       'location', 'southeast');
title('Step responses — tf + step');
