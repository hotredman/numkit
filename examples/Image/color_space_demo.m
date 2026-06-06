% color_space_demo.m
% Round-trip RGB → HSV → RGB and exploit the HSV intermediate to rotate
% the image's hue by 120 degrees (cyclic in H, no contrast change).

import compat.*;

clear
close all

% Synthesize a 128x128 RGB image: three coloured strips with a soft
% vertical luminance gradient on top, so the hue shift is obvious.
H = 128; W = 128;
rgb = zeros(H, W, 3);
rgb(:, 1:42,    1) = 0.85;   rgb(:, 1:42,    2) = 0.20;   rgb(:, 1:42,    3) = 0.20;
rgb(:, 43:84,   1) = 0.20;   rgb(:, 43:84,   2) = 0.85;   rgb(:, 43:84,   3) = 0.20;
rgb(:, 85:128,  1) = 0.20;   rgb(:, 85:128,  2) = 0.20;   rgb(:, 85:128,  3) = 0.85;

ramp = linspace(0.4, 1.0, H)';
for c = 1:3
    rgb(:, :, c) = rgb(:, :, c) .* ramp;
end

% Round-trip + hue rotation.
hsv = rgb2hsv(rgb);
roundtrip = hsv2rgb(hsv);

shifted = hsv;
shifted(:,:,1) = mod(shifted(:,:,1) + 1/3, 1.0);   % +120° hue
rgb_shifted = hsv2rgb(shifted);

figure;
subplot(1,3,1); imshow(rgb);          title('Original RGB');
subplot(1,3,2); imshow(roundtrip);    title('RGB→HSV→RGB (identity)');
subplot(1,3,3); imshow(rgb_shifted);  title('Hue + 120°');
sgtitle('Color-space conversion');
