% FFT Spectrum — Frequency content of a signal
% Build a signal from two sine waves and reveal them with fft.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

Fs = 256;
t = 0:1/Fs:1-1/Fs;
N = length(t);

% 30 Hz + 80 Hz
x = 0.7*sin(2*pi*30*t) + 0.3*sin(2*pi*80*t);

X = fft(x);
mag = abs(X(1:N/2+1)) / N;
mag(2:end-1) = 2 * mag(2:end-1);
f = (0:N/2) * Fs / N;

figure(1)
plot(t(1:128), x(1:128))
title('Time Domain (first 0.5 s)')
xlabel('Time (s)')
ylabel('Amplitude')
grid on

figure(2)
plot(f, mag, 'r-', 'LineWidth', 1.5)
title('Single-Sided Spectrum')
xlabel('Frequency (Hz)')
ylabel('|X(f)|')
grid on
