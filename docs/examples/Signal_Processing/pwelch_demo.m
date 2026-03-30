% Power Spectrum — Welch's method (pwelch)
% Estimate PSD of a signal with two tones in noise.
clear
close all

Fs = 1024;
t = 0:1/Fs:2-1/Fs;
N = length(t);

% Two tones at 120 Hz and 250 Hz + noise
x = 0.8*sin(2*pi*120*t) + 0.4*sin(2*pi*250*t) + 0.5*randn(1, N);

win = hamming(256);
[Pxx, F] = pwelch(x, win, 128, 512);

% Convert to dB
PdB = 10 * log10(Pxx + 1e-12);

figure
plot(F/pi * (Fs/2), PdB, 'b-', 'LineWidth', 1.5)
title('Welch PSD Estimate')
xlabel('Frequency (Hz)')
ylabel('Power/Frequency (dB)')
grid on
