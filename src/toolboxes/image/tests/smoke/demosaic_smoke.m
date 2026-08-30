clear

% demosaic — Bayer → RGB via Malvar-He-Cutler 2004.
% Reference engine: MATLAB R2025b Image Processing Toolbox.

fprintf('== constant inputs (DC preservation) ==\n');
I = uint8(128*ones(8,8));
RGB = demosaic(I, 'rggb');
fprintf('  rggb const 128: center [%d %d %d] (expect 128 128 128)\n', RGB(4,4,1), RGB(4,4,2), RGB(4,4,3));
RGB = demosaic(I, 'bggr');
fprintf('  bggr const 128: center [%d %d %d]\n', RGB(4,4,1), RGB(4,4,2), RGB(4,4,3));
RGB = demosaic(I, 'grbg');
fprintf('  grbg const 128: center [%d %d %d]\n', RGB(4,4,1), RGB(4,4,2), RGB(4,4,3));
RGB = demosaic(I, 'gbrg');
fprintf('  gbrg const 128: center [%d %d %d]\n', RGB(4,4,1), RGB(4,4,2), RGB(4,4,3));

fprintf('\n== distinguishable RGGB mosaic ==\n');
I = uint8(zeros(8,8));
I(1:2:end,1:2:end) = 100;   % R
I(1:2:end,2:2:end) = 50;    % G1
I(2:2:end,1:2:end) = 60;    % G2
I(2:2:end,2:2:end) = 200;   % B
RGB = demosaic(I, 'rggb');
fprintf('  B-pixel (4,4): R=%d (e 100)  G=%d (e 55)  B=%d (e 200)\n', RGB(4,4,1), RGB(4,4,2), RGB(4,4,3));
fprintf('  R-pixel (3,3): R=%d (e 100)  G=%d (e 55)  B=%d (e 200)\n', RGB(3,3,1), RGB(3,3,2), RGB(3,3,3));
fprintf('  G-pixel (1,2) boundary: R=%d (e 95)\n', RGB(1,2,1));
fprintf('  G-pixel (2,1) boundary: R=%d (e 105)\n', RGB(2,1,1));

RGB = demosaic(I, 'bggr');
fprintf('\n  bggr (4,4): R=%d (e 200)  G=%d (e 55)  B=%d (e 100)\n', RGB(4,4,1), RGB(4,4,2), RGB(4,4,3));

RGB = demosaic(I, 'grbg');
fprintf('  grbg (4,4): R=%d (e 100)  G=%d (e 200)  B=%d (e 110)\n', RGB(4,4,1), RGB(4,4,2), RGB(4,4,3));

RGB = demosaic(I, 'gbrg');
fprintf('  gbrg (4,4): R=%d (e 110)  G=%d (e 200)  B=%d (e 100)\n', RGB(4,4,1), RGB(4,4,2), RGB(4,4,3));

fprintf('\n== smooth gradient ==\n');
[X,Y] = meshgrid(1:8, 1:8);
I = uint8(X*8 + Y);
RGB = demosaic(I, 'rggb');
fprintf('  (3,3): R=%d G=%d B=%d (e 27 27 27)\n', RGB(3,3,1), RGB(3,3,2), RGB(3,3,3));
fprintf('  (5,5): R=%d G=%d B=%d (e 45 45 45)\n', RGB(5,5,1), RGB(5,5,2), RGB(5,5,3));

fprintf('\n== uint16 class preserved ==\n');
I16 = uint16(1000*ones(6,6));
RGB = demosaic(I16, 'rggb');
fprintf('  class=%s   center [%d %d %d] (expect 1000)\n', class(RGB), RGB(3,3,1), RGB(3,3,2), RGB(3,3,3));

fprintf('\n== BitsPerSample NV (no-op) ==\n');
RGB = demosaic(I16, 'rggb', 'BitsPerSample', 12);
fprintf('  bps=12 center R=%d (expect 1000)\n', RGB(3,3,1));
