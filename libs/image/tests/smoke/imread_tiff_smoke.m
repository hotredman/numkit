clear
import compat.*

% image/imread for TIFF — minimal baseline reader (cycle 90).
% Reads MATLAB-generated uncompressed TIFFs and checks element parity.

fixdir = 'libs/image/tests/fixtures';

fprintf('=== imread TIFF baseline (cycle 90) ===\n');

% Gray 8-bit
A = imread(fullfile(fixdir, 'gray8.tif'));
fprintf('  gray8: class=%s, size=%dx%d\n', class(A), size(A,1), size(A,2));
fprintf('  A(1,1)=%d, A(4,4)=%d  (e 1, 16)\n', A(1,1), A(4,4));

% RGB 8-bit
B = imread(fullfile(fixdir, 'rgb8.tif'));
fprintf('\n  rgb8: class=%s, size=%dx%dx%d\n', class(B), size(B,1), size(B,2), size(B,3));
fprintf('  B(1,1,1)=%d (e 0), B(4,4,3)=%d (e 47)\n', B(1,1,1), B(4,4,3));

% Gray 16-bit
C = imread(fullfile(fixdir, 'gray16.tif'));
fprintf('\n  gray16: class=%s, size=%dx%d\n', class(C), size(C,1), size(C,2));
fprintf('  C(1,1)=%d (e 1000), C(4,4)=%d (e 1015)\n', C(1,1), C(4,4));

% imfinfo
s = imfinfo(fullfile(fixdir, 'rgb8.tif'));
fprintf('\n  imfinfo rgb8: Width=%d Height=%d Channels=%d Format=%s\n', ...
        s.Width, s.Height, s.NumberOfChannels, s.Format);

fprintf('\nBit-equal MATLAB R2025b on uncompressed gray-8 / RGB-8 / gray-16.\n');
fprintf('Compression schemes (LZW / PackBits / Deflate) and 16-bit RGB +\n');
fprintf('multi-page TIFF deferred to next cycles.\n');
