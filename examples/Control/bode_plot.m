% bode_plot.m
% Bode magnitude/phase plots for a low-pass and a band-pass filter.
% bode returns (mag, phase, w) in linear units / radians; convert
% manually for the canonical dB / degrees view.

import compat.*;

clear
close all

% Two systems.
G_lp = tf(1,        [1, 1]);             % 1st-order LPF, wc = 1 rad/s
G_bp = tf([1, 0],   [1, 0.6, 1]);        % band-pass, w0 = 1, Q ~ 1.7

w = logspace(-2, 2, 400);

[mag_lp, ph_lp] = bode(G_lp, w);
[mag_bp, ph_bp] = bode(G_bp, w);

% bode returns 3-D arrays (out × in × w); squeeze for SISO.
mag_lp = squeeze(mag_lp); ph_lp = squeeze(ph_lp);
mag_bp = squeeze(mag_bp); ph_bp = squeeze(ph_bp);

figure;
subplot(2, 1, 1);
semilogx(w, 20*log10(mag_lp), w, 20*log10(mag_bp), 'LineWidth', 1.5);
grid on;
ylabel('|G| (dB)');
legend('1st-order LPF', 'band-pass', 'location', 'southwest');
title('Bode plot');

subplot(2, 1, 2);
semilogx(w, ph_lp, w, ph_bp, 'LineWidth', 1.5);
grid on;
xlabel('\omega (rad/s)'); ylabel('phase (deg)');
