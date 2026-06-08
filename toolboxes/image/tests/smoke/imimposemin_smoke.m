clear

import compat.*

% Image with two basins on a plateau:
%   plateau = 10, basin A at (2,2) value 3, basin B at (2,5) value 6.
% Without imposition, regional minima are {(2,2), (2,5)}.
% MATLAB R2025b / Octave convention: markers in J become -Inf,
% non-marker pixels are lifted by h = (max-min)/1000 (for float).
I = double([
    10 10 10 10 10 10 10;
    10  3 10 10  6 10 10;
    10 10 10 10 10 10 10]);
h = (max(I(:)) - min(I(:))) / 1000;       % expected lift = 0.007

% --- Mark only basin A. Imposition should erase basin B. ---
BW1 = false(3, 7);
BW1(2, 2) = true;
J1 = imimposemin(I, BW1);
[~, n1] = bwlabel(imregionalmin(J1));
fprintf('--- imimposemin(I, BW only at (2,2)) ---\n');
fprintf('  regional-min count of J1 = %d (expect 1 — only marker)\n', n1);
fprintf('  J1(2,2) = %.4f (expect -Inf)\n', J1(2, 2));
fprintf('  J1(2,5) = %.4f (expect 10+h = %.4f — basin B lifted to plateau)\n', ...
    J1(2, 5), 10 + h);
fprintf('  J1(1,1) = %.4f (expect 10+h = %.4f — plateau lifted by h)\n\n', ...
    J1(1, 1), 10 + h);

% --- Mark a non-minimum location. ---
BW2 = false(3, 7);
BW2(1, 1) = true;
J2 = imimposemin(I, BW2);
[~, n2] = bwlabel(imregionalmin(J2));
fprintf('--- imimposemin(I, BW only at (1,1)) ---\n');
fprintf('  regional-min count of J2 = %d (expect 1 — only the new marker)\n', n2);
fprintf('  J2(1,1) = %.4f (expect -Inf)\n', J2(1, 1));
% basin A is 8-adjacent to (1,1) marker, so the path goes directly
% through one pixel — its lifted value (3+h) is the path's "highest
% hill". Basin A keeps that value but is no longer a regional min
% because the marker neighbour (-Inf) is strictly smaller.
fprintf('  J2(2,2) = %.4f (expect 3+h = %.4f — kept value but no longer min)\n', ...
    J2(2, 2), 3 + h);
fprintf('  J2(2,5) = %.4f (expect 10+h = %.4f — basin B erased to plateau)\n\n', ...
    J2(2, 5), 10 + h);

% --- Mark BOTH minima. Both survive as -Inf, no new ones. ---
BW3 = false(3, 7);
BW3(2, 2) = true; BW3(2, 5) = true;
J3 = imimposemin(I, BW3);
[~, n3] = bwlabel(imregionalmin(J3));
fprintf('--- imimposemin(I, BW at both minima) ---\n');
fprintf('  regional-min count of J3 = %d (expect 2 — both markers)\n', n3);
fprintf('  J3(2,2) = %.4f, J3(2,5) = %.4f (both expect -Inf)\n', ...
    J3(2, 2), J3(2, 5));
