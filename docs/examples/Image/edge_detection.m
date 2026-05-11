% edge_detection.m
% Compare edge detectors (sobel / prewitt / canny / log) on a synthetic
% scene: a filled square + circle on a slowly varying gradient.

import compat.*;

clear
close all

% Synthetic 128x128 scene with sharp shapes on a soft gradient.
[X, Y] = meshgrid(1:128, 1:128);
img = 0.4 + 0.003 * X;            % background gradient
sq  = (X >= 25 & X <= 60 & Y >= 25 & Y <= 60);
cir = ((X - 90).^2 + (Y - 90).^2) <= 25^2;
img(sq | cir) = 0.9;

% Apply each detector.
e_sobel   = edge(img, 'sobel');
e_prewitt = edge(img, 'prewitt');
e_canny   = edge(img, 'canny');
e_log     = edge(img, 'log');

% 4-up display.
figure;
subplot(2,2,1); imshow(e_sobel);   title('Sobel');
subplot(2,2,2); imshow(e_prewitt); title('Prewitt');
subplot(2,2,3); imshow(e_canny);   title('Canny');
subplot(2,2,4); imshow(e_log);     title('LoG');
sgtitle('Edge detectors on the same scene');
