clear
import compat.*

% regionprops now reports the per-pixel list fields, matching MATLAB R2025b
% (both previously threw 'non-existent field'):
%   PixelIdxList : column vector of COLUMN-MAJOR 1-based linear indices of
%                  the region's pixels, sorted ascending.
%   PixelList    : P-by-2 [x y] = [col row] (1-based), in the same order.
% Neither is part of the basic default property set.

BW = false(4,4);
BW(2,2) = true; BW(2,3) = true; BW(3,3) = true;   % pixels (2,2),(2,3),(3,3)

s = regionprops(BW, 'PixelIdxList', 'PixelList');
fprintf('PixelIdxList (%dx%d) = ', size(s.PixelIdxList,1), size(s.PixelIdxList,2));
fprintf('%d ', s.PixelIdxList);
fprintf('  (expect 6 10 11 as a column)\n');

pl = s.PixelList;
fprintf('PixelList (%dx%d):\n', size(pl,1), size(pl,2));
for i = 1:size(pl,1)
    fprintf('   [x=%d y=%d]\n', pl(i,1), pl(i,2));
end
fprintf('   (expect [2 2] [3 2] [3 3])\n');

% Two regions: indices are per-region, each sorted column-major.
BW2 = false(5,5);
BW2(1,1) = true; BW2(3,3) = true; BW2(3,4) = true; BW2(4,4) = true;
s2 = regionprops(BW2, 'PixelIdxList');
fprintf('two regions: r1 = '); fprintf('%d ', s2(1).PixelIdxList);
fprintf(' r2 = '); fprintf('%d ', s2(2).PixelIdxList);
fprintf('  (expect r1=1, r2=13 18 19)\n');

% Basic default still returns only Area/Centroid/BoundingBox.
sb = regionprops(BW);
fprintf('basic default fields = %s\n', strjoin(fieldnames(sb), ','));
