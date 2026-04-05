% Envelope Detection — Hilbert transform
% Extract the amplitude envelope of an AM signal.
clear
close all

Fs = 1000;
t = 0:1/Fs:0.5-1/Fs;

% AM signal: carrier 100 Hz, modulation 5 Hz
mod_signal = 1 + 0.7 * cos(2*pi*5*t);
x = mod_signal .* cos(2*pi*100*t);

% Extract envelope
env = envelope(x);

figure
hold on
plot(t, x, 'b-')
plot(t, env, 'r-', 'LineWidth', 2)
plot(t, -env, 'r-', 'LineWidth', 2)
hold off
title('AM Signal with Hilbert Envelope')
xlabel('Time (s)')
ylabel('Amplitude')
legend('signal', 'envelope')
grid on
