% filter_blur_sharpen.m
% Build classic image kernels via fspecial and apply them with imfilter:
%   • gaussian — soft blur
%   • sobel    — directional edge response
%   • laplacian — second-derivative edge response
% Synthetic input: a checkerboard with a smooth bias.

import compat.*;

clear
close all

% 128x128 checkerboard (32-px squares) + soft luminance ramp.
[X, Y] = meshgrid(1:128, 1:128);
img = double(mod(floor((X-1)/32) + floor((Y-1)/32), 2));
img = 0.7 * img + 0.003 * X;       % add ramp so blur is visible

% Three kernels.
gK = fspecial('gaussian', [9 9], 2.0);
sK = fspecial('sobel');
lK = fspecial('laplacian', 0.2);

g = imfilter(img, gK);
s = imfilter(img, sK);
l = imfilter(img, lK);

figure;
subplot(2,2,1); imshow(img,    []); title('Original');
subplot(2,2,2); imshow(g,      []); title('Gaussian blur (\sigma=2)');
subplot(2,2,3); imshow(abs(s), []); title('Sobel response (|·|)');
subplot(2,2,4); imshow(abs(l), []); title('Laplacian response (|·|)');
sgtitle('fspecial + imfilter');
