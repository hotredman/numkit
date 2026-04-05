% Zero-Phase Filter — filtfilt vs filter
% filtfilt applies the filter forward and backward,
% eliminating phase distortion.
clear
close all

Fs = 500;
t = 0:1/Fs:0.2-1/Fs;

% Sharp pulse + noise
x = zeros(1, length(t));
x(40:60) = 1;
x = x + 0.1 * randn(1, length(t));

% 4th-order Butterworth lowpass
[b, a] = butter(4, 0.2);

y_filt    = filter(b, a, x);
y_filtfilt = filtfilt(b, a, x);

figure
hold on
plot(t, x, 'b-')
plot(t, y_filt, 'r-', 'LineWidth', 1.5)
plot(t, y_filtfilt, 'g-', 'LineWidth', 2)
hold off
title('filter vs filtfilt (zero-phase)')
xlabel('Time (s)')
ylabel('Amplitude')
legend('noisy', 'filter', 'filtfilt')
grid on
