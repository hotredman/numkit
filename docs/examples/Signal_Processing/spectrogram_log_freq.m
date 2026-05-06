% Spectrogram with logarithmic frequency axis + log color scale
%
% Builds a tonal mixture whose components sit at octave intervals
% (50/100/200/400/800/1600 Hz) so the difference between linear and
% log frequency axes is dramatic — on log axes they're evenly spaced,
% on linear the lower octaves collapse into a single bottom row.
%
% Two log knobs available:
%
%   1. `colorscale('log')` BEFORE `imagesc` — the engine bakes log10
%      into the quantization at construction. Useful for power
%      spectrograms whose values span many orders of magnitude:
%      without it the colormap range gets eaten by outliers and
%      everything looks washed out.
%
%   2. Log Y axis — toggle via right-click on the figure ("Axes →
%      Y axis · log"). The image resamples to a log-distributed grid
%      on every zoom, so frequencies are evenly spaced visually
%      regardless of how zoomed-in you are.
%
% Performance notes:
%   - For a 16k×42k spectrogram (192 kHz × 1 hour, window 32768) the
%     engine quantizes to uint8 + builds a lazy LOD pyramid → ~890 MB
%     in memory and instant zoom-out via the pyramid. The example
%     here uses a smaller workload that runs in ~1 sec for fast
%     iteration; scale it up by changing Fs and dur below.

import compat.*;

clear
close all

Fs   = 8000;       % sample rate
dur  = 6;          % seconds
t    = 0:1/Fs:dur-1/Fs;
N    = length(t);

% Tonal mixture: harmonics at octave intervals, each amplitude-modulated
% so the spectrogram has visible structure rather than constant lines.
freqs = [50 100 200 400 800 1600];
amps  = [1.0 0.7 0.5 0.6 0.4 0.3];
modFs = [0.3 0.4 0.6 0.5 0.8 1.0];   % each tone breathes at its own rate

x = zeros(1, N);
for k = 1:length(freqs)
    envelope = 0.5 + 0.5 * sin(2*pi * modFs(k) * t);
    x = x + amps(k) * envelope .* sin(2*pi * freqs(k) * t);
end

% Sprinkle a tiny noise floor — exposes how log color compresses the
% gap between strong tones and silence.
x = x + 0.001 * randn(1, N);

% STFT — modest window, 75% overlap.
winLen   = 1024;
win      = hamming(winLen);
noverlap = 768;
nfft     = 2048;

[S, F, T] = spectrogram(x, win, noverlap, nfft);

% Power (|S|^2). Don't take log here — let the engine bake it in via
% colorscale('log') below. Cleaner: hover then shows "10^x ≈ y" so the
% reported value is in original-power units, not in already-logged dB.
P = abs(S).^2;

% Hz axis: F came out in normalised radians/sample; convert to Hz.
freq_hz = F * (Fs / 2) / pi;
time_s  = T / Fs;

figure
colorscale('log');               % ← bake log10 into the quantization
imagesc(time_s, freq_hz, P)
yscale('log');                   % ← log frequency axis (call after imagesc,
                                 %    just like MATLAB's set(gca,'YScale','log'))
colormap('viridis')
title('Octave-spaced tones · log color · log Y')
xlabel('Time (s)')
ylabel('Frequency (Hz)')

% Octaves now spread evenly across the panel. You can toggle log/linear
% axes interactively via right-click on the figure → "Axes → X/Y axis
% · log". Image resamples without reloading data; pan/zoom in log space
% stays interactive.
