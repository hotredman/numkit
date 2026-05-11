% spectrogram_chirp.m
% Build a linearly-swept sinusoid (chirp) and visualise its time-
% varying spectrum with spectrogram.  The diagonal stripe in the
% time-frequency plane is the chirp's instantaneous frequency.

import compat.*;

clear
close all

fs = 8000;
t  = (0:1/fs:2)';                  % 2 seconds
% Chirp: 200 Hz at t=0, 2000 Hz at t=2s.
f0 = 200; f1 = 2000;
x  = chirp(t, f0, t(end), f1);

% Spectrogram: 256-sample Hann window, 75% overlap, 256-pt FFT.
win = 256; ovl = 192; nfft = 256;
S = spectrogram(x, hamming(win), ovl, nfft, fs);
P = abs(S).^2;

% Build matching frequency / time axes.
F = linspace(0, fs/2, size(P, 1));
hop = win - ovl;
T = ((0:size(P, 2) - 1) * hop + win/2) / fs;

figure;
subplot(2, 1, 1);
plot(t, x);
xlabel('time (s)'); ylabel('amplitude');
title(sprintf('Chirp %d → %d Hz', f0, f1));
grid on;

subplot(2, 1, 2);
imagesc(T, F, 10*log10(P + 1e-12));
axis xy; colorbar;
xlabel('time (s)'); ylabel('frequency (Hz)');
title('Spectrogram (dB)');
