% Long Waveform — a few seconds of an audio-rate signal
% Plots a decaying 440 Hz tone sampled at 48 kHz (~240k points). The
% renderer downsamples the visible range to the plot width, so the whole
% waveform draws instantly and stays smooth as you pan and zoom in.
import compat.*;

clear
close all

fs = 48000;                 % sample rate (Hz)
t  = 0:1/fs:5;              % 5 seconds  →  ~240k samples
y  = sin(2*pi*440*t) .* exp(-t/2);

figure
plot(t, y)
title('Decaying 440 Hz tone — 5 s @ 48 kHz (~240k samples)')
xlabel('Time (s)')
ylabel('Amplitude')
grid on
