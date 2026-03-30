% Convolution — Signal smoothing with conv
% Convolve a noisy signal with a moving-average kernel.
clear
close all

% Noisy square pulse
N = 200;
x = zeros(1, N);
x(50:150) = 1;
x = x + 0.15 * randn(1, N);

% Moving-average kernel (length 11)
h = ones(1, 11) / 11;

y_full = conv(x, h, 'full');
y_same = conv(x, h, 'same');

figure(1)
hold on
plot(1:N, x, 'b-')
plot(1:N, y_same, 'r-', 'LineWidth', 2)
hold off
title('Moving-Average Smoothing (conv, same)')
xlabel('Sample')
ylabel('Amplitude')
legend('noisy', 'smoothed')
grid on
