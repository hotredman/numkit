clear

import compat.*

% label2rgb — colorize a labelled image with an explicit colormap.

% --- basic: 3 labels + zero background ---
fprintf('--- label2rgb(L, cmap) ---\n');
L     = [0 1 1 0 2 2 0 3 3
         0 1 1 0 2 2 0 3 3];
cmap  = [0 0 0; 0.5 0.5 0.5; 0.125 0.125 0.125];
RGB   = label2rgb(L, cmap);
fprintf('size: %s\n', mat2str(size(RGB)));
disp(double(RGB(:, :, 1)));
fprintf('  expect: bg=255 (white), label1=0, label2=128, label3=32\n\n');

% --- with explicit cyan background ---
fprintf('--- label2rgb(L, cmap, [0 1 1]) ---\n');
RGBc  = label2rgb(L, cmap, [0 1 1]);
fprintf('first pixel R/G/B = [%d %d %d] (expect [0 255 255])\n', ...
        RGBc(1, 1, 1), RGBc(1, 1, 2), RGBc(1, 1, 3));
fprintf('  expect: bg pixels are cyan, labels keep cmap colors\n\n');

% --- larger label image with synthesized colormap ---
fprintf('--- label2rgb(uint8 L, cmap) ---\n');
L2    = uint8([1 1 2 2; 3 3 0 0; 0 0 4 4]);
cmap2 = [1 0 0; 0 1 0; 0 0 1; 1 1 0];   % red green blue yellow
RGB2  = label2rgb(L2, cmap2);
fprintf('size %s\n', mat2str(size(RGB2)));
fprintf('label1 R = %d (expect 255)\n', RGB2(1, 1, 1));
fprintf('label2 G = %d (expect 255)\n', RGB2(1, 3, 2));
fprintf('label0 (bg) RGB = [%d %d %d] (expect [255 255 255])\n', ...
        RGB2(2, 3, 1), RGB2(2, 3, 2), RGB2(2, 3, 3));
