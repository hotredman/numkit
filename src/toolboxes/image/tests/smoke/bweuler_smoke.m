clear

% bweuler — Euler number (objects − holes) of a binary image.

% --- Octave-source reference: rectangle with 2 holes → -1 ---
A = zeros(10, 10);
A(2:9, 3:8) = 1;
A(4, 4) = 0;
A(8, 8) = 0;   % corner — not a hole
A(6, 6) = 0;
fprintf('rect with 2 holes (n=8) = %d (expect -1)\n', bweuler(A));

% --- two diagonally adjacent squares: n=4 → 2 objects, n=8 → 1 ---
B = zeros(10, 10);
B(2:4, 2:4) = 1;
B(5:8, 5:8) = 1;
fprintf('diag squares n=4 = %d (expect 2)\n', bweuler(B, 4));
fprintf('diag squares n=8 = %d (expect 1)\n', bweuler(B, 8));
fprintf('diag squares default = %d (expect 1)\n', bweuler(B));

% --- single isolated pixel ---
C = zeros(5, 5);
C(3, 3) = 1;
fprintf('single pixel = %d (expect 1)\n', bweuler(C));

% --- empty ---
fprintf('empty = %d (expect 0)\n', bweuler(zeros(5)));
