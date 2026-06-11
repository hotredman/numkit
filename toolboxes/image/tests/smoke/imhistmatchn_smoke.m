clear

import compat.*

% imhistmatchn — N-D variant of imhistmatch. Single histogram across
% all elements (volume semantics). Aliased to imhistmatch in numkit.

% --- 2-D: same as imhistmatch ---
fprintf('--- imhistmatchn (2-D) ---\n');
A      = uint8(reshape(0:99, [10 10]));
ref    = uint8(reshape(100:199, [10 10]));
B      = imhistmatchn(A, ref);
fprintf('range: [%d, %d]  mean: %.2f (ref mean: %.2f)\n', ...
        min(B(:)), max(B(:)), mean(double(B(:))), mean(double(ref(:))));
fprintf('  expect: histogram-shifted toward ref, mean ≈ ref mean\n\n');

% --- 3-D volume: full single-histogram across all pages ---
fprintf('--- imhistmatchn (3-D volume) ---\n');
V      = uint8(cat(3, [10 20; 30 40], [50 60; 70 80], [90 100; 110 120]));
Vref   = uint8(cat(3, [200 210; 220 230], [200 210; 220 230], [200 210; 220 230]));
W      = imhistmatchn(V, Vref);
fprintf('size: %s\n', mat2str(size(W)));
fprintf('mean(V)=%.1f mean(W)=%.1f mean(Vref)=%.1f\n', ...
        mean(double(V(:))), mean(double(W(:))), mean(double(Vref(:))));
fprintf('  expect: W mean shifted toward Vref mean (~215)\n\n');

% --- explicit nbins ---
fprintf('--- imhistmatchn(A, ref, 32) ---\n');
B32    = imhistmatchn(A, ref, 32);
fprintf('range: [%d, %d]\n', min(B32(:)), max(B32(:)));
fprintf('  expect: similar mapping at 32-bin resolution\n');
