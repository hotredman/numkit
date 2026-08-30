clear

% bwtraceboundary — Moore-Neighbor boundary tracing.
% Bit-exact MATLAB R2025b.

BW = false(7, 7);
BW(2:6, 2:6) = true;

fprintf('=== 5x5 square, fstep=E, clockwise (default) ===\n');
B = bwtraceboundary(BW, [2 2], 'E');
fprintf('%d pts (expect 17)\n', size(B,1));
fprintf('  first three: [%d %d; %d %d; %d %d]\n', ...
    B(1,1), B(1,2), B(2,1), B(2,2), B(3,1), B(3,2));

fprintf('\n=== limit N=5 ===\n');
B = bwtraceboundary(BW, [2 2], 'E', 8, 5);
fprintf('%d pts (expect 5)\n', size(B,1));

fprintf('\n=== conn=4 ===\n');
B = bwtraceboundary(BW, [2 2], 'E', 4);
fprintf('%d pts (expect 17 — same perimeter for square)\n', size(B,1));

fprintf('\n=== fstep=S (boundary fallback to E) ===\n');
B = bwtraceboundary(BW, [2 2], 'S');
fprintf('%d pts (expect 17)\n', size(B,1));
fprintf('  B(2,:) = [%d %d] (expect [2 3])\n', B(2,1), B(2,2));

fprintf('\n=== L-shape (concave) ===\n');
LL = false(8, 8);
LL(2:5, 2:3) = true;
LL(2:3, 4:6) = true;
B = bwtraceboundary(LL, [2 2], 'E');
fprintf('%d pts (expect 14)\n', size(B,1));

fprintf('\n=== single pixel ===\n');
S1 = false(5, 5); S1(3, 3) = true;
B = bwtraceboundary(S1, [3 3], 'E');
fprintf('%d pts (expect 2 -- [P; P])\n', size(B,1));
