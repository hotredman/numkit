clear

% Stem Plot — Discrete signal
% Visualize a sampled sinusoid as vertical lines with markers.
% Stem is the standard way to display discrete-time signals.

n = 0:20;
y = sin(2 * pi * n / 10);
figure
stem(n, y)
title('Discrete Sinusoid')
xlabel('Sample n')
ylabel('Amplitude')
grid on
