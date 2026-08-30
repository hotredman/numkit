clear

% mean2 / std2 / corr2 — image statistics.

A = [1 2 3; 4 5 6; 7 8 9];
B = A + 1;

fprintf('--- mean2 / std2 ---\n');
fprintf('mean2(A) = %.6f (expect 5)\n',     mean2(A));
fprintf('std2(A)  = %.6f (expect 2.581989)\n', std2(A));   % sqrt(60/9)

fprintf('\n--- corr2(A, B) ---\n');
fprintf('corr2(A, B) = %.6f (expect 1.0 — A and B are linearly related)\n', ...
        corr2(A, B));

fprintf('\n--- corr2 with anti-correlated ---\n');
C = -A;
fprintf('corr2(A, C) = %.6f (expect -1.0)\n', corr2(A, C));

fprintf('\n--- corr2 with random ---\n');
D = [1 4 7; 2 5 8; 3 6 9];   % sequential, but transposed
fprintf('corr2(A, D) = %.6f\n', corr2(A, D));
