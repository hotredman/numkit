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

% --- DEEP-PROBE c180: default jet colormap (no map arg) ---
fprintf('\n--- label2rgb(L) default jet ---\n');
yd = double(label2rgb([1 2 0; 0 1 2]));
fprintf('label1 = [%d %d %d] (expect [0 0 255])\n',   yd(1,1,1), yd(1,1,2), yd(1,1,3));
fprintf('label2 = [%d %d %d] (expect [0 255 255])\n', yd(1,2,1), yd(1,2,2), yd(1,2,3));
fprintf('zero   = [%d %d %d] (expect [255 255 255])\n', yd(1,3,1), yd(1,3,2), yd(1,3,3));

% --- named colormap string ('hsv') ---
fprintf('\n--- label2rgb(L, ''hsv'') ---\n');
yh = double(label2rgb([1 2 0; 0 1 2], 'hsv'));
fprintf('label1 = [%d %d %d] (expect [255 0 0])\n',   yh(1,1,1), yh(1,1,2), yh(1,1,3));
fprintf('label2 = [%d %d %d] (expect [0 255 255])\n', yh(1,2,1), yh(1,2,2), yh(1,2,3));

% --- ColorSpec / named-color zerocolor ---
fprintf('\n--- zerocolor color-string ---\n');
yk = double(label2rgb([1 2 0; 0 1 2], 'jet', 'k'));
yr = double(label2rgb([1 2 0; 0 1 2], 'jet', 'r'));
yg = double(label2rgb([1 2 0; 0 1 2], 'jet', 'green'));
fprintf('zero ''k''     = [%d %d %d] (expect [0 0 0])\n',   yk(1,3,1), yk(1,3,2), yk(1,3,3));
fprintf('zero ''r''     = [%d %d %d] (expect [255 0 0])\n', yr(1,3,1), yr(1,3,2), yr(1,3,3));
fprintf('zero ''green'' = [%d %d %d] (expect [0 255 0])\n', yg(1,3,1), yg(1,3,2), yg(1,3,3));

% --- order 'shuffle' deferred (throws cleanly) ---
fprintf('\n--- order ''shuffle'' (deferred) ---\n');
try
    label2rgb([1 2 3 4], 'jet', 'k', 'shuffle');
    fprintf('  ERROR: shuffle did not throw\n');
catch
    fprintf('  shuffle throws as expected (needs swb2712 stream)\n');
end
