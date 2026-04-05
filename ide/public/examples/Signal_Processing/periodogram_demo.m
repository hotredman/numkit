% Periodogram — Basic spectral estimate
% Compare raw periodogram with a Hamming-windowed version.
clear
close all

Fs = 512;
t = 0:1/Fs:1-1/Fs;
N = length(t);

% 50 Hz tone in noise
x = sin(2*pi*50*t) + 0.8*randn(1, N);

% Raw periodogram (rectangular window)
[Pxx1, F1] = periodogram(x);

% Windowed periodogram
w = hamming(N);
[Pxx2, F2] = periodogram(x, w, 1024);

figure(1)
plot(F1/pi*(Fs/2), 10*log10(Pxx1 + 1e-12), 'b-')
title('Periodogram (rectangular window)')
xlabel('Frequency (Hz)')
ylabel('Power (dB)')
grid on

figure(2)
plot(F2/pi*(Fs/2), 10*log10(Pxx2 + 1e-12), 'r-')
title('Periodogram (Hamming window)')
xlabel('Frequency (Hz)')
ylabel('Power (dB)')
grid on
