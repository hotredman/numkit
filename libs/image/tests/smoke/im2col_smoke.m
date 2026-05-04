clear

import compat.*

% --- Sliding mode on a small lattice ---
% A is 4x4 column-major:
%    1  5  9 13
%    2  6 10 14
%    3  7 11 15
%    4  8 12 16
A = reshape(1:16, [4 4]);
fprintf('--- A (4x4) ---\n');
disp(A);

% 2x2 sliding: positions (H-1)*(W-1) = 3*3 = 9 cols, 4 rows.
% First column = top-left 2x2 block flattened column-major:
%   [1 2 5 6]'  (column 1: 1,2; column 2: 5,6)
B = im2col(A, [2 2]);
fprintf('--- im2col(A, [2 2]) sliding ---\n');
fprintf('  size(B) = [%d %d] (expect [4 9])\n', size(B, 1), size(B, 2));
fprintf('  B(:,1) = [%d %d %d %d]'' (expect [1 2 5 6]'')\n', ...
    B(1,1), B(2,1), B(3,1), B(4,1));
fprintf('  B(:,5) = [%d %d %d %d]'' (expect [6 7 10 11]'' — middle 2x2)\n', ...
    B(1,5), B(2,5), B(3,5), B(4,5));
fprintf('  B(:,9) = [%d %d %d %d]'' (expect [11 12 15 16]'' — bottom-right)\n\n', ...
    B(1,9), B(2,9), B(3,9), B(4,9));

% --- Distinct mode: 4x4 partitions evenly into 2x2 ---
B2 = im2col(A, [2 2], 'distinct');
fprintf('--- im2col(A, [2 2], ''distinct'') ---\n');
fprintf('  size(B2) = [%d %d] (expect [4 4])\n', size(B2, 1), size(B2, 2));
fprintf('  B2(:,1) = [%d %d %d %d]'' (expect [1 2 5 6]'')\n', ...
    B2(1,1), B2(2,1), B2(3,1), B2(4,1));
fprintf('  B2(:,2) = [%d %d %d %d]'' (expect [3 4 7 8]'')\n', ...
    B2(1,2), B2(2,2), B2(3,2), B2(4,2));
fprintf('  B2(:,3) = [%d %d %d %d]'' (expect [9 10 13 14]'')\n', ...
    B2(1,3), B2(2,3), B2(3,3), B2(4,3));
fprintf('  B2(:,4) = [%d %d %d %d]'' (expect [11 12 15 16]'')\n\n', ...
    B2(1,4), B2(2,4), B2(3,4), B2(4,4));

% --- Distinct mode with zero-padding (5x4 with [2 2] tiles → 3 row-tiles) ---
A3 = reshape(1:20, [5 4]);
B3 = im2col(A3, [2 2], 'distinct');
fprintf('--- im2col(5x4, [2 2], ''distinct'') ---\n');
fprintf('  size(B3) = [%d %d] (expect [4 6] — 3*2 tiles)\n', ...
    size(B3, 1), size(B3, 2));
% Last row-tile of column 1 takes rows 5..6, but row 6 doesn't exist:
%   tile (3,1): A(5,1) = 5, A(6,1) = pad = 0, A(5,2) = 10, A(6,2) = pad = 0
% column index = 0*3 + 2 = 2 (third column of B3)
fprintf('  B3(:,3) = [%d %d %d %d]'' (expect [5 0 10 0]'' — bottom-edge pad)\n\n', ...
    B3(1,3), B3(2,3), B3(3,3), B3(4,3));

% --- Sliding for convolution: each block × kernel sums the conv response ---
% Verify that im2col + matrix-vector-multiply = imfilter(valid).
K = [1 2; 3 4] / 10;     % small 2x2 kernel
expected = zeros(3, 3);
for r = 1:3
    for c = 1:3
        block = A(r:r+1, c:c+1);
        % MATLAB convolution flips the kernel; im2col-based filter does
        % correlation. Use correlation here to keep parity with im2col.
        expected(r, c) = sum(sum(block .* K));
    end
end
B = im2col(A, [2 2]);
% K flattened column-major: K(:) = [K(1,1) K(2,1) K(1,2) K(2,2)] = [.1 .3 .2 .4]
result_vec = K(:)' * B;
result = reshape(result_vec, [3 3]);
fprintf('--- im2col convolution-equivalence check ---\n');
fprintf('  max|reshape(K(:)''*im2col(A)) - explicit| = %.6e (expect 0)\n', ...
    max(max(abs(result - expected))));

% --- Edge case: block size equals image size → 1 column ---
B_full = im2col(A, [4 4]);
fprintf('--- im2col(4x4 image, [4 4]) ---\n');
fprintf('  size(B_full) = [%d %d] (expect [16 1])\n', ...
    size(B_full, 1), size(B_full, 2));
fprintf('  max|B_full - A(:)| = %.6e (expect 0)\n', ...
    max(abs(B_full - A(:))));

% --- Bad arg: block larger than image (sliding) ---
ok = false;
try
    im2col(A, [5 5]);
catch
    ok = true;
end
fprintf('  oversized-sliding raises = %d (expect 1)\n', ok);
