% Spectrogram with peak markers + text annotations
%
% Demonstrates the imagesc-overlay pipeline. After `imagesc(...)` and
% `hold on` you can layer scatter / line / text on the same axes — they
% all share the panel's pan/zoom state and the colorbar stays the
% legend for the underlying heatmap.
%
% This example builds a chord of three sustained tones, marks the
% expected peak frequencies with red dots, draws a horizontal track
% line at one frequency, and labels each peak.

import compat.*;
clear
close all

Fs   = 8000;
dur  = 4;
t    = 0:1/Fs:dur-1/Fs;
N    = length(t);

% Three tones — the "peaks" we'll annotate.
peak_freqs = [220 440 880];
x = zeros(1, N);
for k = 1:length(peak_freqs)
    x = x + 0.6 * sin(2*pi * peak_freqs(k) * t);
end
x = x + 0.001 * randn(1, N);   % thin noise floor

[S, F, T] = spectrogram(x, hamming(1024), 768, 2048);
P        = abs(S).^2;
freq_hz  = F * (Fs / 2) / pi;
time_s   = T / Fs;

figure
colorscale('log');
imagesc(time_s, freq_hz, P)
yscale('log');
colormap('viridis')
title('Three-tone chord · markers + labels')
xlabel('Time (s)')
ylabel('Frequency (Hz)')

% Hold the heatmap and overlay annotations.
hold on
% Mark each tone's location at mid-time with a red dot.
mid_t = (time_s(1) + time_s(end)) / 2;
for k = 1:length(peak_freqs)
    scatter(mid_t, peak_freqs(k), 80, 'r', 'filled')
end

% Draw a horizontal track at the highest tone — illustrates that line()
% over imagesc tracks pan/zoom + log axes correctly.
plot([time_s(1), time_s(end)], [peak_freqs(end), peak_freqs(end)], 'w--')

% Label each peak. text() takes (x, y, str, 'Color', c, 'FontSize', s).
for k = 1:length(peak_freqs)
    text(mid_t * 0.55, peak_freqs(k) * 1.05, ...
         sprintf('%d Hz', peak_freqs(k)), ...
         'Color', 'white', 'FontSize', 11);
end
