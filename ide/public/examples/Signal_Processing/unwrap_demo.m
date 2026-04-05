% Phase Unwrap — Correct wrapped phase angles
% unwrap removes 2*pi discontinuities from phase data.
clear
close all

% Linearly increasing phase that wraps
t = linspace(0, 4*pi, 200);
phase_wrapped = mod(t + pi, 2*pi) - pi;

phase_unwrapped = unwrap(phase_wrapped);

figure(1)
plot(t, phase_wrapped, 'b-', 'LineWidth', 1.5)
title('Wrapped Phase')
xlabel('Sample')
ylabel('Phase (rad)')
grid on

figure(2)
hold on
plot(t, phase_unwrapped, 'r-', 'LineWidth', 1.5)
plot(t, t, 'b--', 'LineWidth', 1)
hold off
title('Unwrapped Phase')
xlabel('Sample')
ylabel('Phase (rad)')
legend('unwrapped', 'true')
grid on
