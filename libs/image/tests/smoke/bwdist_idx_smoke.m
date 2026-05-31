clear
import compat.*

% bwdist now returns the 2nd output IDX (the "feature transform"): for each
% pixel, the column-major 1-based LINEAR INDEX of the nearest foreground
% pixel. Class uint32; ties break to the lowest linear index (MATLAB
% R2025b). Previously [D,IDX]=bwdist(...) threw 'Index exceeds array
% dimensions' (only D was returned).

BW = logical([0 0 0 0; 0 1 0 0; 0 0 0 0; 0 0 0 1]);   % seeds at linidx 6 and 16
[D, IDX] = bwdist(BW);
fprintf('--- [D,IDX] = bwdist(BW) (euclidean) ---\n');
fprintf('class(IDX)=%s\n', class(IDX));
fprintf('IDX(1,1)=%d IDX(2,2)=%d IDX(4,4)=%d IDX(3,4)=%d sum=%d\n', ...
        IDX(1,1), IDX(2,2), IDX(4,4), IDX(3,4), sum(IDX(:)));
fprintf('  (expect 6, 6, 16, 16, sum 126)\n');

[Dc, IDXc] = bwdist(BW, 'cityblock');
[Dk, IDXk] = bwdist(BW, 'chessboard');
fprintf('cityblock sum(IDX)=%d  chessboard sum(IDX)=%d  (expect 126, 126)\n', ...
        sum(IDXc(:)), sum(IDXk(:)));

% Tie-break: a pixel equidistant from two seeds points to the LOWER index.
BW2 = false(5,5); BW2(1,1) = true; BW2(5,5) = true;
[D2, IDX2] = bwdist(BW2);
fprintf('--- tie-break ---\n');
fprintf('IDX2(1,5)=%d IDX2(5,1)=%d IDX2(5,5)=%d  (expect 1, 1, 25)\n', ...
        IDX2(1,5), IDX2(5,1), IDX2(5,5));
