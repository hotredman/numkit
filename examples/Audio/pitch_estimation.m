% pitch_estimation.m
% Build a synthetic voice-like signal whose fundamental frequency
% sweeps from 110 Hz (A2) to 220 Hz (A3), and recover f0 via pitch().

import compat.*;

clear
close all

fs = 8000;
t  = (0:1/fs:1)';

% Time-varying f0.
f0 = linspace(110, 220, numel(t))';

% Phase = ∫ 2π f0 dt.
phase = 2 * pi * cumsum(f0) / fs;

% Signal: fundamental + 2nd + 3rd harmonics (vowel-like).
x = sin(phase) + 0.5 * sin(2 * phase) + 0.3 * sin(3 * phase);

% Estimate pitch (default NCF — normalised correlation function).
% MATLAB returns one f0 estimate per analysis frame; build a matching
% time axis assuming the default 50 ms window / 10 ms hop.
f0_est = pitch(x, fs);
hop_s  = 0.010;
t_est  = ((0:numel(f0_est) - 1) * hop_s + 0.025)';   % centre of window

figure;
subplot(2, 1, 1);
plot(t, x); xlabel('time (s)'); ylabel('amplitude');
title('Synthetic harmonic signal'); grid on;

subplot(2, 1, 2);
plot(t, f0, 'k--', 'LineWidth', 1.2);
hold on;
plot(t_est, f0_est, 'bo-', 'MarkerSize', 4);
hold off;
xlabel('time (s)'); ylabel('f_0 (Hz)');
legend('true f_0', 'pitch() estimate', 'location', 'northwest');
title('Pitch tracking — sweep 110 → 220 Hz'); grid on;
