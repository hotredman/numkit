% Spectrogram — Time-frequency analysis with imagesc
% Chirp signal whose frequency sweeps from low to high,
% displayed as a spectrogram heatmap.
clear
close all

Fs = 512;
t = 0:1/Fs:2-1/Fs;
N = length(t);

% Linear chirp: 10 Hz -> 200 Hz over 2 seconds
x = sin(2*pi * (10*t + 95*t.^2));

% Compute spectrogram
winLen = 128;
win = hamming(winLen);
noverlap = 96;
nfft = 256;

[S, F, T] = spectrogram(x, win, noverlap, nfft);

% Power in dB
P = zeros(size(S, 1), size(S, 2));
for r = 1:size(S, 1)
    for c = 1:size(S, 2)
        P(r, c) = abs(S(r, c))^2;
    end
end
P = 10 * log10(P + 1e-12);

figure
imagesc(T / Fs, F / pi * (Fs/2), P)
colormap('jet')
title('Spectrogram of Linear Chirp')
xlabel('Time (s)')
ylabel('Frequency (Hz)')
