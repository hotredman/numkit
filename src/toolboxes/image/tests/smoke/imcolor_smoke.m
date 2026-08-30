clear

% Test image: pure red, pure green, pure blue, white, black, gray
% as a 6×3 colormap (cleaner than 3D for testing)
RGB = [1 0 0; 0 1 0; 0 0 1; 1 1 1; 0 0 0; 0.5 0.5 0.5];

% rgb2hsv
fprintf('--- rgb2hsv ---\n');
disp(rgb2hsv(RGB));
fprintf('  expect rows: red=[0 1 1], green=[1/3 1 1], blue=[2/3 1 1],\n');
fprintf('               white=[0 0 1], black=[0 0 0], gray=[0 0 0.5]\n\n');

% Round-trip RGB → HSV → RGB
fprintf('--- hsv2rgb(rgb2hsv(RGB)) round-trip ---\n');
disp(hsv2rgb(rgb2hsv(RGB)));
fprintf('  expect: same as input\n\n');

% rgb2ycbcr
fprintf('--- rgb2ycbcr ---\n');
disp(rgb2ycbcr([1 0 0; 0 1 0; 0 0 1]));
fprintf('  expect (BT.601, double):\n');
fprintf('    red:   [0.2569 0.5020 1.0000]   (Y=0.2569, Cb=0.5020, Cr=1.0)... wait\n');
fprintf('    matlab: [81.481/255  90.797/255  240/255] = [0.3196 0.3559 0.9412]\n\n');

% Round-trip RGB → YCbCr → RGB
fprintf('--- ycbcr2rgb(rgb2ycbcr(RGB)) round-trip ---\n');
disp(ycbcr2rgb(rgb2ycbcr(RGB)));
fprintf('  expect: same as input within roundoff\n\n');

% rgb2xyz on white = D65
fprintf('--- rgb2xyz([1 1 1]) (sRGB white) ---\n');
disp(rgb2xyz([1 1 1]));
fprintf('  expect: D65 white = [0.95047 1.0 1.08883]\n\n');

% rgb2lab on red, green, blue
fprintf('--- rgb2lab on RGB primaries ---\n');
disp(rgb2lab([1 0 0; 0 1 0; 0 0 1]));
fprintf('  red:   L≈53.24,  a≈80.09,   b≈67.20\n');
fprintf('  green: L≈87.74,  a≈-86.18,  b≈83.18\n');
fprintf('  blue:  L≈32.30,  a≈79.20,   b≈-107.86\n\n');

% Round-trip rgb→lab→rgb
fprintf('--- lab2rgb(rgb2lab(RGB)) round-trip ---\n');
disp(lab2rgb(rgb2lab([1 0 0; 0 1 0; 0 0 1])));
fprintf('  expect: identity within roundoff\n\n');

% Black-box: rgb2lab(white) = [100 0 0]
fprintf('--- rgb2lab([1 1 1]) (white) ---\n');
disp(rgb2lab([1 1 1]));
fprintf('  expect: [100 0 0]\n');
