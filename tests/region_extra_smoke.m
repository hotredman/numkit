import compat.*

% --- Two simple square blobs, well separated ---
BW = false(7, 12);
% Square 1: rows 2-3, cols 2-4 (3x2 = 6 px), bbox = [1.5, 1.5, 3, 2]
BW(2:3, 2:4) = true;
% Square 2: rows 5-6, cols 7-10 (4x2 = 8 px), bbox = [6.5, 4.5, 4, 2]
BW(5:6, 7:10) = true;

stats = regionprops(BW);
fprintf('--- regionprops on 2 blobs ---\n');
fprintf('  numel(stats) = %d (expect 2)\n', numel(stats));
fprintf('  stats(1).Area = %d (expect 6)\n', stats(1).Area);
fprintf('  stats(2).Area = %d (expect 8)\n', stats(2).Area);
fprintf('  stats(1).Centroid = [%.2f, %.2f] (expect [3.0, 2.5])\n', ...
    stats(1).Centroid(1), stats(1).Centroid(2));
fprintf('  stats(2).Centroid = [%.2f, %.2f] (expect [8.5, 5.5])\n', ...
    stats(2).Centroid(1), stats(2).Centroid(2));
fprintf('  stats(1).BoundingBox = [%.2f %.2f %.0f %.0f] (expect [1.5 1.5 3 2])\n', ...
    stats(1).BoundingBox(1), stats(1).BoundingBox(2), ...
    stats(1).BoundingBox(3), stats(1).BoundingBox(4));
fprintf('  stats(2).BoundingBox = [%.2f %.2f %.0f %.0f] (expect [6.5 4.5 4 2])\n\n', ...
    stats(2).BoundingBox(1), stats(2).BoundingBox(2), ...
    stats(2).BoundingBox(3), stats(2).BoundingBox(4));

% --- regionprops with selected props only ---
sa = regionprops(BW, 'Area');
fprintf('--- regionprops(BW, ''Area'') ---\n');
fprintf('  has Area only = %d (expect 1)\n', isfield(sa, 'Area'));

% --- bwboundaries on a single 3x3 square ---
B = false(5, 5);
B(2:4, 2:4) = true;
bd = bwboundaries(B);
fprintf('\n--- bwboundaries on a 3x3 square ---\n');
fprintf('  numel(bd) = %d (expect 1)\n', numel(bd));
fprintf('  size(bd{1}) = %dx%d (expect 9x2 — 8 perimeter + closing)\n', ...
    size(bd{1}, 1), size(bd{1}, 2));
fprintf('  first row of bd{1} = [%d %d] (expect [2 2] — top-left)\n', ...
    bd{1}(1, 1), bd{1}(1, 2));
fprintf('  last row of bd{1} = [%d %d] (expect [2 2] — closure)\n', ...
    bd{1}(end, 1), bd{1}(end, 2));

% --- bwboundaries on 2 disconnected blobs ---
bd2 = bwboundaries(BW);
fprintf('\n--- bwboundaries on 2 blobs ---\n');
fprintf('  numel(bd2) = %d (expect 2)\n', numel(bd2));
fprintf('  size(bd2{1}, 1) = %d (square 3x2 perimeter+closure)\n', size(bd2{1}, 1));
fprintf('  size(bd2{2}, 1) = %d (square 4x2 perimeter+closure)\n', size(bd2{2}, 1));

% --- regionprops on a label image directly (no relabel) ---
L = zeros(5, 5);
L(2:4, 2:4) = 1;
L(5, 5) = 2;
sl = regionprops(L);
fprintf('\n--- regionprops on label image ---\n');
fprintf('  numel(sl) = %d (expect 2)\n', numel(sl));
fprintf('  sl(1).Area = %d (expect 9)\n', sl(1).Area);
fprintf('  sl(2).Area = %d (expect 1)\n', sl(2).Area);
