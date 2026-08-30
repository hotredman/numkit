clear

% conv(A,B,'same') returns the central part the SAME SIZE AS A (length na),
% starting at floor(nb/2) of the full convolution. Even-kernel offset + the
% na<nb length were fixed 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== ''same'' offset for even kernels ===\n');
fprintf('conv([1 2 3 4],[1 1],''same'')   = %s (expect [3 5 7 4], NOT [1 3 5 7])\n', ...
        mat2str(conv([1 2 3 4],[1 1],'same')));
fprintf('conv([1 2 3],[1 1],''same'')     = %s (expect [3 5 3])\n', ...
        mat2str(conv([1 2 3],[1 1],'same')));
fprintf('conv([1 2 3 4 5],[1 1 1 1],''same'') = %s (expect [6 10 14 12 9])\n', ...
        mat2str(conv([1 2 3 4 5],[1 1 1 1],'same')));

fprintf('\n=== odd kernel unchanged ===\n');
fprintf('conv([1 2 3 4 5],[1 1 1],''same'') = %s (expect [3 6 9 12 9])\n', ...
        mat2str(conv([1 2 3 4 5],[1 1 1],'same')));

fprintf('\n=== first arg shorter than kernel -> length na ===\n');
h = conv([1 2],[1 1 1 1 1],'same');
fprintf('conv([1 2],[1 1 1 1 1],''same'') = %s  numel=%d (expect [3 3], numel 2)\n', ...
        mat2str(h), numel(h));

fprintf('\n=== full / valid regress ===\n');
fprintf('conv([1 2 3],[1 1])          = %s (expect [1 3 5 3])\n', mat2str(conv([1 2 3],[1 1])));
fprintf('conv([1 2 3 4],[1 1],''valid'') = %s (expect [3 5 7])\n', mat2str(conv([1 2 3 4],[1 1],'valid')));
