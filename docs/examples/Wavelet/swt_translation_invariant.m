% swt_translation_invariant.m
% Stationary (undecimated) wavelet transform: unlike DWT, swt keeps
% every detail level the same length as the input — useful for
% denoising and feature extraction where time alignment matters.

import compat.*;

clear
close all

N = 256;
t = (0:N-1)' / N;
% Short bumps + low-freq carrier — see how the detail levels localise
% the bumps regardless of where they are.
x = sin(2*pi*3*t);
x(60:80)   = x(60:80)   + 1.2;
x(170:190) = x(170:190) - 1.0;

L = 4;
sw = swt(x, L, 'db2');     % returns (L+1) × N: rows 1..L = details, row L+1 = approx

figure;
subplot(L+1, 1, 1); plot(t, x); title('Input signal'); grid on;
for k = 1:L
    subplot(L+1, 1, k+1);
    plot(t, sw(k, :));
    title(sprintf('SWT detail level %d', k));
    grid on;
end
sgtitle('Stationary wavelet transform (db2)');
