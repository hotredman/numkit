clear
import compat.*

% regionprops now supports the grayscale intensity-image form
% regionprops(BW, I, props), matching MATLAB R2025b (a numeric 2nd arg
% previously threw 'property names must be strings'):
%   MeanIntensity    : mean of I over the region (double).
%   MaxIntensity     : max of I over the region.
%   MinIntensity     : min of I over the region.
%   WeightedCentroid : sum(I.*[x y]) / sum(I).
%   PixelValues      : column vector of I values in PixelIdxList order.
% These come only with an intensity image; not part of the basic default set.

BW = false(4,4);
BW(2,2) = true; BW(2,3) = true; BW(3,3) = true;   % pixel values 6,10,11
I = reshape(1:16,4,4);

s = regionprops(BW, I, 'MeanIntensity', 'MaxIntensity', 'MinIntensity', ...
                'WeightedCentroid', 'PixelValues');
fprintf('Mean=%.4f Max=%.1f Min=%.1f   (expect 9, 11, 6)\n', ...
        s.MeanIntensity, s.MaxIntensity, s.MinIntensity);
fprintf('WeightedCentroid=[%.6f %.6f]   (expect 2.777778, 2.407407)\n', ...
        s.WeightedCentroid(1), s.WeightedCentroid(2));
fprintf('PixelValues (%dx%d) = ', size(s.PixelValues,1), size(s.PixelValues,2));
fprintf('%d ', s.PixelValues); fprintf('  (expect 6 10 11 as a column)\n');

% uint8 intensity image.
I8 = uint8(reshape(0:10:150,4,4));
s8 = regionprops(BW, I8, 'MeanIntensity', 'MaxIntensity');
fprintf('uint8 Mean=%.4f Max=%g   (expect 80, 100)\n', s8.MeanIntensity, s8.MaxIntensity);

% A string 2nd arg is still a property; intensity fields need an image.
sa = regionprops(BW, 'Area');
fprintf('string-prop Area=%d   (string 2nd arg = property name)\n', sa.Area);
sd = regionprops(BW, I);
fprintf('default-with-image fields = %s   (still the basic 3)\n', strjoin(fieldnames(sd), ','));
