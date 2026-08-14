% Image I/O Round-trip — imwrite, iminfo, imread
% Generate a synthetic color pattern, save it to PNG and JPEG image files,
% query metadata via imfinfo, read it back via imread, and display the result.

clear
close all

% 1. Create a 128x128 synthetic RGB color image (color gradient circle)
[X, Y] = meshgrid(linspace(-1, 1, 128), linspace(-1, 1, 128));
R = sqrt(X.^2 + Y.^2);
theta = atan2(Y, X);

% Map radius and angle to RGB channels
red   = uint8(255 * (0.5 + 0.5 * sin(3 * theta)));
green = uint8(255 * (0.5 + 0.5 * cos(3 * theta)));
blue  = uint8(255 * max(0, min(1, 1 - R)));

img_rgb = zeros(128, 128, 3, 'uint8');
img_rgb(:,:,1) = red;
img_rgb(:,:,2) = green;
img_rgb(:,:,3) = blue;

% 2. Save image to PNG file
filename = 'demo_image.png';
imwrite(img_rgb, filename);

% 3. Inspect image metadata with imfinfo
info = imfinfo(filename);
fprintf('Image Format: %s\n', info.Format);
fprintf('Image Size:   %d x %d x %d\n', info.Height, info.Width, info.NumberOfSamples);

% 4. Read back the image
img_loaded = imread(filename);

% 5. Visualize original vs loaded image
figure;
subplot(1, 2, 1);
imshow(img_rgb);
title('Original RGB Pattern');

subplot(1, 2, 2);
imshow(img_loaded);
title('Loaded from PNG File');
sgtitle('Image File I/O (imwrite / imread / imfinfo)');
