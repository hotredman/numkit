% mel_spectrogram.m
% Compute and display a mel-scale spectrogram — the standard
% perception-aware spectral view used as input to many audio ML
% models. Compares with the plain linear-frequency spectrogram.

import compat.*;

clear
close all

fs = 16000;
t  = (0:1/fs:2)';
% Two-tone with slow amplitude modulation + a sweep.
x = sin(2*pi*440*t) .* (0.5 + 0.5 * sin(2*pi*2*t)) + ...
    0.5 * chirp(t, 200, t(end), 4000);

% Linear spectrogram via spectrogram (returns complex S; build P).
win = 512; ovl = 384; nfft = 1024;
S = spectrogram(x, hamming(win), ovl, nfft, fs);
P_lin = abs(S).^2;
F_lin = linspace(0, fs/2, size(P_lin, 1));
hop = win - ovl;
T_lin = ((0:size(P_lin, 2) - 1) * hop + win/2) / fs;

% Mel-scale spectrogram (default parameters).
Mel = melSpectrogram(x, fs);
T_mel = linspace(0, t(end), size(Mel, 2));
F_mel = linspace(1, size(Mel, 1), size(Mel, 1));

figure;
subplot(2, 1, 1);
imagesc(T_lin, F_lin, 10*log10(P_lin + 1e-12));
axis xy; colorbar;
ylabel('Hz'); title('Linear-frequency spectrogram (dB)');

subplot(2, 1, 2);
imagesc(T_mel, F_mel, 10*log10(Mel + 1e-12));
axis xy; colorbar;
xlabel('time (s)'); ylabel('Mel band index');
title('Mel-spectrogram (dB)');
