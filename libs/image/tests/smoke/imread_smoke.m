import compat.*

% --- Build a tiny 2x3 RGB image, write to PNG, read back ---
A = uint8(zeros(2, 3, 3));
% Pixel layout (row, col) → R G B:
%   (1,1)=red    (1,2)=green  (1,3)=blue
%   (2,1)=yellow (2,2)=magenta (2,3)=cyan
A(1,1,:) = uint8([255   0   0]);
A(1,2,:) = uint8([  0 255   0]);
A(1,3,:) = uint8([  0   0 255]);
A(2,1,:) = uint8([255 255   0]);
A(2,2,:) = uint8([255   0 255]);
A(2,3,:) = uint8([  0 255 255]);

imwrite(A, 'tests/fixtures/_tiny_rgb.png');
B = imread('tests/fixtures/_tiny_rgb.png');

fprintf('--- imwrite + imread round-trip on PNG ---\n');
fprintf('  size(B) = %dx%dx%d (expect 2x3x3)\n', size(B,1), size(B,2), size(B,3));
fprintf('  class(B) = %s (expect uint8)\n', class(B));
fprintf('  max|A - B| = %d (expect 0)\n', max(max(max(abs(double(A) - double(B))))));
fprintf('\n');
fprintf('  B(1,1,1..3) = [%d, %d, %d]   (expect [255, 0, 0]   top-left red)\n', ...
    B(1,1,1), B(1,1,2), B(1,1,3));
fprintf('  B(2,3,1..3) = [%d, %d, %d]   (expect [0, 255, 255] bottom-right cyan)\n\n', ...
    B(2,3,1), B(2,3,2), B(2,3,3));

% --- Grayscale round-trip ---
G = uint8([0 85 170 255; 255 170 85 0]);
imwrite(G, 'tests/fixtures/_tiny_gray.png');
G2 = imread('tests/fixtures/_tiny_gray.png');
fprintf('--- grayscale round-trip ---\n');
fprintf('  size(G2) = %dx%d (expect 2x4)\n', size(G2,1), size(G2,2));
fprintf('  G2 = \n'); disp(G2);
fprintf('  max|G - G2| = %d (expect 0)\n\n', max(max(abs(double(G) - double(G2)))));

% --- BMP round-trip ---
imwrite(A, 'tests/fixtures/_tiny_rgb.bmp');
B_bmp = imread('tests/fixtures/_tiny_rgb.bmp');
fprintf('--- BMP round-trip ---\n');
fprintf('  max|A - B_bmp| = %d (expect 0, BMP is lossless)\n\n', ...
    max(max(max(abs(double(A) - double(B_bmp))))));

% --- Missing-file error ---
try
    imread('tests/fixtures/_does_not_exist.png');
    fprintf('--- BUG: missing file did not error\n');
catch err
    fprintf('--- missing file correctly errored\n');
end

% --- Unsupported extension on imwrite ---
try
    imwrite(A, 'tests/fixtures/_tiny_rgb.xyz');
    fprintf('--- BUG: bad extension did not error\n');
catch err
    fprintf('--- bad extension correctly errored\n');
end
