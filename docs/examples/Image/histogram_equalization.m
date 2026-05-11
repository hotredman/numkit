% histogram_equalization.m
% Boost contrast on a low-dynamic-range synthetic image via histeq.
% Compare the input histogram (clustered in a narrow band) with the
% output (spread across the full range).

import compat.*;

clear
close all

% Build a low-contrast 128x128 grayscale image: smooth gradient with
% a couple of bumps, all mapped into the [0.3, 0.55] band.
[X, Y] = meshgrid(linspace(0, 1, 128));
raw = 0.5 * X + 0.3 * Y + 0.2 * sin(6*pi*X) .* cos(4*pi*Y);
raw = raw - min(raw(:));
raw = raw / max(raw(:));
low = 0.3 + 0.25 * raw;            % squashed to [0.3, 0.55]

eq = histeq(low);                  % full-range output

figure;
subplot(2,2,1); imshow(low);  title('Low-contrast input');
subplot(2,2,2); imshow(eq);   title('After histeq');
subplot(2,2,3); imhist(low);  title('Input histogram (narrow)');
subplot(2,2,4); imhist(eq);   title('Output histogram (spread)');
sgtitle('Histogram equalization');
