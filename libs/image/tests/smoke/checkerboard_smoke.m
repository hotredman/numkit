clear

import compat.*

% checkerboard — synthetic checkerboard image generator.

% --- side=1, default 4x4 tiles → 8x8 image ---
fprintf('--- checkerboard(1) ---\n');
B1 = checkerboard(1);
fprintf('size: %s\n', mat2str(size(B1)));
disp(B1);
fprintf('  expect: 8x8; left half alternating 0/1, right half alternating 0/0.7\n\n');

% --- default: side=10, 4x4 tiles → 80x80 ---
fprintf('--- checkerboard() ---\n');
B0 = checkerboard();
fprintf('size: %s, range [%.2f, %.2f]\n', ...
        mat2str(size(B0)), min(B0(:)), max(B0(:)));
fprintf('  expect: 80x80; values in {0, 0.7, 1}\n\n');

% --- explicit (side=2, M=N=3) → 12x12 ---
fprintf('--- checkerboard(2, 3, 3) ---\n');
B2 = checkerboard(2, 3, 3);
fprintf('size: %s\n', mat2str(size(B2)));
fprintf('top-left 2x2 (expect zeros): ');
fprintf('%g ', B2(1:2, 1:2)); fprintf('\n');
fprintf('right half last row max: %.2f (expect 0.7)\n', max(B2(1, 7:12)));

% --- non-square (side=1, M=2, N=4) → 4x8 ---
fprintf('--- checkerboard(1, 2, 4) ---\n');
B3 = checkerboard(1, 2, 4);
fprintf('size: %s\n', mat2str(size(B3)));
disp(B3);
