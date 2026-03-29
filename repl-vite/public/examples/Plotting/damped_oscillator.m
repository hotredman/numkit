clear

% Damped Oscillator — Second-order system response
% Plots an underdamped oscillation with its exponential envelope.

zeta = 0.1;
wn = 2 * pi;
wd = wn * sqrt(1 - zeta^2);

t = linspace(0, 5, 500);
y = exp(-zeta * wn * t) .* cos(wd * t);
envelope = exp(-zeta * wn * t);

figure
hold on
plot(t, y, 'b-', 'LineWidth', 1.5)
plot(t, envelope, 'r--', 'LineWidth', 1)
plot(t, -envelope, 'r--', 'LineWidth', 1)
hold off
title('Underdamped Oscillator (zeta = 0.1)')
xlabel('Time (s)')
ylabel('Amplitude')
legend('response', 'envelope')
grid on
