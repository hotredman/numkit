% dwt_decompose.m
% Single-level discrete wavelet transform: split a sine + chirp signal
% into approximation (low-freq) and detail (high-freq) bands using
% Daubechies-4 (db4).

import compat.*;

clear
close all

fs = 1024;
t  = (0:1/fs:1-1/fs)';
x  = sin(2*pi*10*t) + 0.5 * sin(2*pi*120*t .* t);   % low + chirp

[cA, cD] = dwt(x, 'db4');

figure;
subplot(3,1,1); plot(t, x);          title('Signal: 10 Hz sine + chirp');
                xlabel('t (s)'); grid on;
subplot(3,1,2); plot(cA);            title('Approximation cA (low-freq)');
                xlabel('coef index'); grid on;
subplot(3,1,3); plot(cD);            title('Detail cD (high-freq, the chirp)');
                xlabel('coef index'); grid on;
sgtitle('1-level DWT with db4');
