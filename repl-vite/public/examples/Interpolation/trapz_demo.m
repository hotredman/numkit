% Trapz Demo — Numerical integration
% Approximate the area under a curve using trapz.
clear
close all

% Integrate sin(x) from 0 to pi (exact = 2)
x = linspace(0, pi, 100);
y = sin(x);
area = trapz(x, y);

disp('Integral of sin(x) from 0 to pi:')
disp(area)

% Visualize
figure
plot(x, y, 'b-', 'LineWidth', 2)
title('sin(x) — trapz area ~ 2.0')
xlabel('x')
ylabel('sin(x)')
grid on
