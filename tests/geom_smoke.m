import compat.*

% --- imresize 2× nearest on a 2×2 ramp ---
A = uint8([10 20; 30 40]);
B = imresize(A, 2, 'nearest');
fprintf('--- imresize(2x2, scale=2, nearest) ---\n');
fprintf('  size(B) = %dx%d (expect 4x4)\n', size(B,1), size(B,2));
fprintf('  B = \n'); disp(B);
fprintf('  expect each input pixel replicated in a 2x2 block\n\n');

% --- imresize bilinear preserves DC ---
G = uint8(50 * ones(8, 8));
G2 = imresize(G, 0.5);
fprintf('--- imresize(constant 50, scale=0.5, bilinear) ---\n');
fprintf('  size(G2) = %dx%d (expect 4x4)\n', size(G2,1), size(G2,2));
fprintf('  mean(G2) = %.2f (expect 50.0)\n\n', mean(double(G2(:))));

% --- imresize explicit size ---
G3 = imresize(G, [3 5]);
fprintf('--- imresize(8x8, [3 5]) ---\n');
fprintf('  size(G3) = %dx%d (expect 3x5)\n\n', size(G3,1), size(G3,2));

% --- imcrop ---
A = uint8([1 2 3 4; 5 6 7 8; 9 10 11 12; 13 14 15 16]);
% rect = [xmin ymin width height] = [2 2 2 2] should give 3x3 block at rows 2-4, cols 2-4
B = imcrop(A, [2 2 2 2]);
fprintf('--- imcrop(4x4, [2 2 2 2]) ---\n');
fprintf('  size(B) = %dx%d (expect 3x3 — width+1, height+1)\n', size(B,1), size(B,2));
fprintf('  B = \n'); disp(B);
fprintf('  expect [6 7 8; 10 11 12; 14 15 16]\n\n');

% --- imtranslate by integer shift on a constant ---
T = imtranslate(uint8(100*ones(5,5)), [1 1]);
fprintf('--- imtranslate(constant, [1 1]) ---\n');
fprintf('  T(1,1) = %d (expect 0, shifted off-edge)\n', T(1,1));
fprintf('  T(3,3) = %d (expect 100, interior preserved)\n', T(3,3));
fprintf('  T(end,end) = %d (expect 100)\n\n', T(end,end));

% --- imrotate 90° on a non-square image, loose bbox ---
% A is 2x4 — rotated 90 CCW becomes 4x2.
A = uint8([1 2 3 4; 5 6 7 8]);
R = imrotate(A, 90, 'nearest');
fprintf('--- imrotate(2x4, 90deg, loose) ---\n');
fprintf('  size(R) = %dx%d (expect 4x2)\n', size(R,1), size(R,2));
fprintf('  R = \n'); disp(R);
fprintf('  expect [4 8; 3 7; 2 6; 1 5] (CCW rotation)\n\n');

% --- imrotate 0° preserves the image ---
R0 = imrotate(A, 0, 'nearest', 'crop');
fprintf('--- imrotate(A, 0, crop) ---\n');
fprintf('  max|A - R0| = %d (expect 0)\n\n', ...
    max(max(abs(double(A) - double(R0)))));

% --- imresize 3-channel image ---
RGB = uint8(zeros(4, 4, 3));
RGB(:,:,1) = 255;
RGB2 = imresize(RGB, 2);
fprintf('--- imresize 3-channel red image ---\n');
fprintf('  size = %dx%dx%d (expect 8x8x3)\n', ...
    size(RGB2,1), size(RGB2,2), size(RGB2,3));
fprintf('  RGB2(4,4,1) = %d (expect 255 in red channel)\n', RGB2(4,4,1));
fprintf('  RGB2(4,4,2) = %d (expect 0 in green channel)\n', RGB2(4,4,2));
