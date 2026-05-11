% eye_diagram.m
% Build a baseband BPSK signal with rectangular pulse shaping, pass it
% through a low-pass to model bandwidth limiting, and overlay 2-symbol
% windows to draw an eye diagram.

import compat.*;

clear
close all

rng(0);

sps = 8;                          % samples per symbol
N   = 200;                        % symbols
bits = randi([0 1], N, 1);
sym  = 2 * bits - 1;              % {-1, +1}
tx   = repelem(sym, sps);         % rectangular pulses

% Bandwidth-limit with a moving-average filter (poor man's low-pass).
h  = ones(sps/2, 1) / (sps/2);
rx = conv(tx, h, 'same');
rx = rx + 0.08 * randn(size(rx));

% Slice into 2-symbol windows (overlay).
win = 2 * sps;
M   = floor(numel(rx) / win) - 1;

figure;
hold on;
for k = 1:M
    seg = rx((k-1)*win + 1 : k*win);
    plot((0:win-1) / sps, seg, 'b', 'LineWidth', 0.5);
end
hold off;
grid on;
xlabel('symbol time'); ylabel('amplitude');
title('Eye diagram — BPSK after pulse shaping + bandwidth limit');
xlim([0 2]);
