% Million-Point Chirp — engine-side downsampling for huge signals
% A linear chirp sampled at 300 kHz over 5 s (~1.5M points). Above ~1M
% samples the engine ships only a compact preview to the viewer and streams
% a freshly-decimated slice each time you zoom — so the plot loads instantly
% and reveals more detail the further in you zoom, without ever moving the
% full signal into the UI.
import compat.*;

clear
close all

fs = 300000;                % 300 kHz
t  = 0:1/fs:5;             % ~1.5M samples
f0 = 20;  f1 = 4000;       % sweep 20 Hz → 4 kHz
k  = (f1 - f0) / t(end);
y  = sin(2*pi*(f0 + 0.5*k*t) .* t);

figure
plot(t, y)
title('Linear chirp — 1.5M samples (zoom in to reveal detail)')
xlabel('Time (s)')
ylabel('Amplitude')
grid on
