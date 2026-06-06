% yyaxis — Dual Y-axis on the same X
% yyaxis('left')  — subsequent plot/scatter/etc. write to the left axis
% yyaxis('right') — switch to the right axis with its own scale
% ylabel/ylim/yscale also route to the active side.
import compat.*;

clear
close all

t = linspace(0, 10, 200);
% Left: a periodic signal in [-1, 1].
y_left  = sin(t);
% Right: a slowly growing exponential in [0, ~e^10/100 ≈ 220].
y_right = exp(t) / 100;

figure
yyaxis('left')
plot(t, y_left)
ylabel('sin(t)')

hold('on')
yyaxis('right')
plot(t, y_right)
ylabel('exp(t)/100')

title('Dual Y-axis: bounded vs growing')
xlabel('t')
grid on
