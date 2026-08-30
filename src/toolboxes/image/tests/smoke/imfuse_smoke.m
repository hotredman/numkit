clear

% imfuse — composite two images for visual comparison.
% Bit-exact MATLAB R2025b parity across all 5 methods.

A = uint8([10 20 30; 40 50 60; 70 80 90]);
B = uint8([90 80 70; 60 50 40; 30 20 10]);

fprintf('=== default (falsecolor green-magenta) ===\n');
C = imfuse(A, B);
fprintf('C(1,1,:) = [%d %d %d] (expect [255 0 255])\n', C(1,1,1), C(1,1,2), C(1,1,3));
fprintf('C(2,2,:) = [%d %d %d] (expect [128 128 128])\n', C(2,2,1), C(2,2,2), C(2,2,3));

fprintf('\n=== blend ===\n');
C = imfuse(A, B, 'blend');
fprintf('C(2,2) = %d (expect 128)\n', C(2,2));

fprintf('\n=== diff ===\n');
C = imfuse(A, B, 'diff');
fprintf('C(1,1) = %d (expect 255)\n', C(1,1));
fprintf('C(2,2) = %d (expect 0)\n', C(2,2));

fprintf('\n=== checkerboard ===\n');
C = imfuse(A, B, 'checkerboard');
fprintf('C(1,1) = %d C(2,2) = %d (expect 0, 128)\n', C(1,1), C(2,2));

fprintf('\n=== montage ===\n');
C = imfuse(A, B, 'montage');
fprintf('size(C) = [%d %d] (expect [3 6])\n', size(C,1), size(C,2));

fprintf('\n=== Scaling: none (no rescale) ===\n');
C = imfuse(A, B, 'blend', 'Scaling', 'none');
fprintf('C(2,2) = %d (expect 50)\n', C(2,2));

fprintf('\n=== ColorChannels: red-cyan ===\n');
C = imfuse(A, B, 'falsecolor', 'ColorChannels', 'red-cyan');
fprintf('C(2,2,:) = [%d %d %d] (expect [128 128 128])\n', C(2,2,1), C(2,2,2), C(2,2,3));

fprintf('\n=== different-size A, B → zero-pad to max ===\n');
A2 = uint8(reshape(1:12, 3, 4));
B2 = uint8(reshape(13:42, 5, 6));
C = imfuse(A2, B2, 'blend');
fprintf('size(C) = [%d %d] (expect [5 6])\n', size(C,1), size(C,2));
