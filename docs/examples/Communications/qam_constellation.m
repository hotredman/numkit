% qam_constellation.m
% Map random bits to a 16-QAM constellation, send through an AWGN
% channel, and compare ideal vs received clouds in the I/Q plane.

import compat.*;

clear
close all

rng(0);
M = 16;                          % 16-QAM (4 bits/symbol)
N = 2000;                        % symbols
data = randi([0 M-1], N, 1);

tx = qammod(data, M);            % unit-normalised constellation

% Send through AWGN at three different SNRs.
SNRs = [25, 15, 8];

figure;
for k = 1:3
    rx = awgn(tx, SNRs(k), 'measured');
    subplot(1, 3, k);
    plot(real(rx), imag(rx), '.', 'MarkerSize', 6);
    hold on;
    plot(real(qammod(0:M-1, M)), imag(qammod(0:M-1, M)), ...
         'r+', 'MarkerSize', 12, 'LineWidth', 2);
    hold off;
    axis equal; grid on;
    xlim([-5 5]); ylim([-5 5]);
    title(sprintf('16-QAM, SNR = %d dB', SNRs(k)));
    xlabel('I'); ylabel('Q');
end
sgtitle('16-QAM through AWGN — constellation degrades with SNR');
