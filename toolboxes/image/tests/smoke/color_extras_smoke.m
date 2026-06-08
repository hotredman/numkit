clear
import compat.*

fprintf('=== rgb2lightness ===\n');
RGB = uint8(reshape([10 20 30 100 150 200 50 80 110 200 220 240], 2, 2, 3));
fprintf('input shape=[%d %d %d] class=%s\n', size(RGB,1), size(RGB,2), size(RGB,3), class(RGB));
L = rgb2lightness(RGB);
fprintf('rgb2lightness output (class=%s):\n', class(L));
disp(L);
fprintf('  expect ~ [55.07 33.40; 73.30 45.27]\n');

% White / black corners
W = uint8(255*ones(1,1,3));
fprintf('rgb2lightness(white) = %g (expect ~100)\n', rgb2lightness(W));
B = uint8(zeros(1,1,3));
fprintf('rgb2lightness(black) = %g (expect 0)\n', rgb2lightness(B));

fprintf('\n=== rgb2ind (inmap form) ===\n');
% Build a simple 4-color palette
cmap = [0 0 0; 1 0 0; 0 1 0; 0 0 1];   % black/red/green/blue
% reshape column-major: page-1 = [1 0 0 0], page-2 = [1 0 0 0], page-3 = [1 0.4 0 0]
% gives pixels (1,1)=white, (2,1)=(0,0,0.4), (1,2)=black, (2,2)=black
RGBd = double(reshape([1 0 0 0.4; 0 1 0 0; 0 0 1 0], 2, 2, 3));
[X, cm] = rgb2ind(RGBd, cmap, 'nodither');
fprintf('rgb2ind output X (class=%s, 0-based MATLAB convention):\n', class(X));
fprintf('  X(1,1)=%d (white→red, idx=1, ties broken to lowest)\n', X(1,1));
fprintf('  X(1,2)=%d (black→black, idx=0)\n', X(1,2));
fprintf('  X(2,1)=%d (0,0,0.4→black, idx=0)\n', X(2,1));
fprintf('  X(2,2)=%d (black→black, idx=0)\n', X(2,2));
fprintf('cmap rows = %d (echoed)\n', size(cm, 1));

% Q form should throw KNOWN GAP
try; [X2, cm2] = rgb2ind(RGBd, 8); catch e; fprintf('\nrgb2ind Q form correctly rejected (KNOWN GAP)\n'); end
try; [X3, cm3] = rgb2ind(RGBd, cmap, 'dither'); catch e; fprintf('rgb2ind dither correctly rejected (KNOWN GAP)\n'); end
