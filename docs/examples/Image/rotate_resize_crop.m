% rotate_resize_crop.m
% Demonstrate the three core geometric operators on the same image:
% imrotate (any angle, bilinear), imresize (scale), imcrop (sub-rect).

import compat.*;

clear
close all

% Synthesize a recognisable image: concentric squares + a diagonal line
% so the rotation is visually obvious.
N = 128;
img = zeros(N, N);
for r = 10:10:50
    img(N/2-r:N/2+r, N/2-r) = 1;        % left side
    img(N/2-r:N/2+r, N/2+r) = 1;        % right side
    img(N/2-r,     N/2-r:N/2+r) = 1;    % top
    img(N/2+r,     N/2-r:N/2+r) = 1;    % bottom
end
for k = 1:N
    img(k, k) = 1;                      % main diagonal
end

rot = imrotate(img, 30, 'bilinear', 'crop');
res = imresize(img, 0.5);
% Crop the central 64x64.
crp = imcrop(img, [33 33 63 63]);

figure;
subplot(2,2,1); imshow(img); title('Original 128×128');
subplot(2,2,2); imshow(rot); title('imrotate(30°, bilinear, crop)');
subplot(2,2,3); imshow(res); title('imresize(0.5)');
subplot(2,2,4); imshow(crp); title('imcrop(central 64×64)');
sgtitle('Geometric transforms');
