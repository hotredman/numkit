% imshow — Display Image
%
% Demonstrates grayscale, explicit-range, uint8, and RGB image
% display. imshow defaults to gray colormap, `axis image off`, and
% an inverted Y (matrix-row 1 at top).

import compat.*;

clear
close all

% --- 1. Grayscale double — default range [0, 1] ---
I = zeros(64, 64);
[X, Y] = meshgrid(linspace(-1, 1, 64), linspace(-1, 1, 64));
I = exp(-(X.^2 + Y.^2) / 0.2);     % Gaussian peak
figure;
imshow(I);
title('Gaussian peak (double in [0, 1])');

% --- 2. Explicit display range — clip the contrast ---
figure;
imshow(I, [0.1 0.6]);
title('Same image, range [0.1 0.6] — narrower contrast');

% --- 3. Auto range — equivalent to imagesc ---
J = X .* exp(-X.^2 - Y.^2);        % bipolar; range ≈ [-0.43, +0.43]
figure;
imshow(J, []);
title('Bipolar data, auto-range (== imagesc)');

% --- 4. uint8 image — range [0, 255] ---
U = uint8(I * 255);
figure;
imshow(U);
title('Same Gaussian as uint8 — default range [0, 255]');

% --- 5. RGB truecolor — M×N×3 ---
R = single(0.5 + 0.5 * sin(2 * pi * X));
G = single(0.5 + 0.5 * sin(2 * pi * Y));
B = single(zeros(size(X)));
figure;
imshow(cat(3, R, G, B));
title('RGB pattern (sin x, sin y, 0)');

% --- 6. Custom colormap via colormap(M) ---
figure;
imshow(I);
M = [0 0 0; 1 0 0; 1 1 0; 1 1 1];   % black → red → yellow → white
colormap(M);
title('Custom 4-stop colormap');

fprintf('imshow demo: 6 figures showcasing grayscale + RGB + colormap\n');
