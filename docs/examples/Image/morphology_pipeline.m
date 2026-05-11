% morphology_pipeline.m
% Walk through the four classical binary-morphology operators on the
% same starting mask:
%   • imdilate — grows white regions
%   • imerode  — shrinks them
%   • imopen   = erode then dilate (removes small white specks)
%   • imclose  = dilate then erode (fills small black holes)

import compat.*;

clear
close all

% Build a 96x96 binary mask: a filled disk + a few small specks +
% a couple of pinhole black holes.
[X, Y] = meshgrid(1:96, 1:96);
mask = (X - 48).^2 + (Y - 48).^2 <= 22^2;
mask(10:13, 10:13) = true;          % isolated speck #1
mask(82:84, 12:14) = true;          % speck #2
mask(70:72, 80:82) = true;          % speck #3
mask(45:47, 45:47) = false;         % pinhole #1
mask(48:50, 52:54) = false;         % pinhole #2

se = strel('disk', 3);
d = imdilate(mask, se);
e = imerode (mask, se);
o = imopen  (mask, se);
c = imclose (mask, se);

figure;
subplot(2,3,1); imshow(mask); title('Input mask');
subplot(2,3,2); imshow(d);    title('imdilate');
subplot(2,3,3); imshow(e);    title('imerode');
subplot(2,3,5); imshow(o);    title('imopen (despeckle)');
subplot(2,3,6); imshow(c);    title('imclose (fill holes)');
sgtitle('Binary morphology pipeline');
