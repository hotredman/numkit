% Cross-Correlation — Detect a delayed copy
% Use xcorr to find the time delay between two signals.
clear
close all

% Original pulse
N = 128;
x = zeros(1, N);
x(20:30) = hann(11)';

% Delayed and noisy copy
delay = 25;
y = zeros(1, N);
y(20+delay:30+delay) = hann(11)';
y = y + 0.05 * randn(1, N);

[c, lags] = xcorr(x, y);

figure(1)
hold on
plot(1:N, x, 'b-', 'LineWidth', 1.5)
plot(1:N, y, 'r-', 'LineWidth', 1.5)
hold off
title('Two Signals (delay = 25 samples)')
xlabel('Sample')
ylabel('Amplitude')
legend('x', 'y (delayed)')
grid on

figure(2)
plot(lags, c, 'b-', 'LineWidth', 1.5)
title('Cross-Correlation')
xlabel('Lag')
ylabel('Rxy')
grid on
