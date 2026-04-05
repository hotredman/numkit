% Resampling — Upsample and downsample signals
% Demonstrates upsample, downsample, and decimate.
clear
close all

% Original signal: 32 samples of a sine
n = 0:31;
x = sin(2*pi*n/16);

% Upsample by 4
x_up = upsample(x, 4);

% Downsample by 2
x_down = downsample(x, 2);

% Decimate by 2 (with anti-alias filter)
x_dec = decimate(x, 2);

figure(1)
stem(n, x)
title('Original (32 samples)')
xlabel('n')
ylabel('x[n]')
grid on

figure(2)
stem(0:length(x_up)-1, x_up)
title('Upsampled by 4 (zero-insertion)')
xlabel('n')
ylabel('x_{up}[n]')
grid on

figure(3)
hold on
stem(0:length(x_down)-1, x_down, 'b-')
stem(0:length(x_dec)-1, x_dec, 'r--')
hold off
title('Downsampled vs Decimated (factor 2)')
xlabel('n')
ylabel('Amplitude')
legend('downsample', 'decimate')
grid on
