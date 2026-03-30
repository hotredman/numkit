% Window Functions — Compare common windows
% Hamming, Hann, Blackman, and Kaiser side by side.
clear
close all

N = 64;
w_ham  = hamming(N);
w_han  = hann(N);
w_blk  = blackman(N);
w_kai  = kaiser(N, 5);

n = 1:N;

figure
hold on
plot(n, w_ham, 'b-', 'LineWidth', 1.5)
plot(n, w_han, 'r-', 'LineWidth', 1.5)
plot(n, w_blk, 'g-', 'LineWidth', 1.5)
plot(n, w_kai, 'm--', 'LineWidth', 1.5)
hold off
title('Window Functions (N = 64)')
xlabel('Sample')
ylabel('Amplitude')
legend('Hamming', 'Hann', 'Blackman', 'Kaiser b=5')
grid on
