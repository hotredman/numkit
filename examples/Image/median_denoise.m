% median_denoise.m
% Salt-and-pepper noise demolished by a 3×3 median filter — a textbook
% example of why median (order-statistics) beats linear filters when
% the noise is impulsive.

import compat.*;

clear
close all

% Build a 128x128 grayscale image with smooth content.
[X, Y] = meshgrid(linspace(-1, 1, 128));
clean = exp(-2*(X.^2 + Y.^2)) + 0.4 * cos(8 * X) .* sin(6 * Y);
clean = (clean - min(clean(:))) / (max(clean(:)) - min(clean(:)));

% Inject 5% salt & 5% pepper.
rng(0);
noisy = clean;
sp = rand(size(clean));
noisy(sp < 0.05)        = 0;        % pepper
noisy(sp > 0.95)        = 1;        % salt

filtered = medfilt2(noisy, [3 3]);

figure;
subplot(1,3,1); imshow(clean);    title('Clean');
subplot(1,3,2); imshow(noisy);    title('+5% salt & pepper');
subplot(1,3,3); imshow(filtered); title('medfilt2 (3×3)');
sgtitle('Median filter denoising');

fprintf('PSNR  noisy vs clean   = %.2f dB\n', psnr(noisy,    clean));
fprintf('PSNR  filtered vs clean = %.2f dB\n', psnr(filtered, clean));
