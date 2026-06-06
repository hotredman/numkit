% denoise_signal.m
% Wavelet denoising via universal-threshold soft-shrinkage on the
% detail coefficients. Demonstrates the classic Donoho-Johnstone
% method: decompose → threshold → reconstruct.

import compat.*;

clear
close all

% Generate a smooth signal + Gaussian noise.
rng(0);
N = 1024;
t = (0:N-1)' / N;
clean = sin(6*pi*t) + 0.6 * sin(20*pi*t) .* exp(-3*t);
noise = 0.3 * randn(N, 1);
noisy = clean + noise;

% Multi-level decomposition (db4, level 4).
L = 4;
[c, l] = wavedec(noisy, L, 'db4');

% Universal threshold (Donoho-Johnstone): T = sigma * sqrt(2 * log N).
% Estimate sigma from finest-level detail (the bulk is noise there).
det1 = detcoef(c, l, 1);
sigma = median(abs(det1)) / 0.6745;
T = sigma * sqrt(2 * log(N));
fprintf('Estimated sigma = %.4f\n', sigma);
fprintf('Universal T     = %.4f\n', T);

% Soft-threshold every detail coefficient (leave the approximation).
nA = l(1);
cT = c;
cT(nA+1:end) = sign(c(nA+1:end)) .* max(abs(c(nA+1:end)) - T, 0);

denoised = waverec(cT, l, 'db4');

figure;
subplot(3,1,1); plot(t, clean);    title('Clean signal');     grid on;
subplot(3,1,2); plot(t, noisy);    title('+ Gaussian noise'); grid on;
subplot(3,1,3); plot(t, denoised); title('After wavelet soft-threshold'); grid on;
sgtitle('Wavelet denoising (db4, universal threshold)');

snr_in  = 20*log10(norm(clean) / norm(noisy - clean));
snr_out = 20*log10(norm(clean) / norm(denoised - clean));
fprintf('SNR in  = %.2f dB\n', snr_in);
fprintf('SNR out = %.2f dB\n', snr_out);
