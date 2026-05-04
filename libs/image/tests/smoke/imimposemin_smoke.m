clear

import compat.*

% Image with two basins on a plateau:
%   plateau = 10, basin A at (2,2) value 3, basin B at (2,5) value 6.
% Without imposition, regional minima are {(2,2), (2,5)}.
I = double([
    10 10 10 10 10 10 10;
    10  3 10 10  6 10 10;
    10 10 10 10 10 10 10]);

% --- Mark only basin A. Imposition should erase basin B. ---
BW1 = false(3, 7);
BW1(2, 2) = true;
J1 = imimposemin(I, BW1);
[~, n1] = bwlabel(imregionalmin(J1));
fprintf('--- imimposemin(I, BW only at (2,2)) ---\n');
fprintf('  regional-min count of J1 = %d (expect 1 — only marker)\n', n1);
fprintf('  J1(2,2) = %.1f (expect 2 — sentinel = min(I)-1)\n', J1(2, 2));
fprintf('  J1(2,5) = %.1f (expect 10 — basin B raised to plateau)\n', ...
    J1(2, 5));
fprintf('  J1(1,1) = %.1f (expect 10 — plateau unchanged)\n\n', J1(1, 1));

% --- Mark a non-minimum location. The marker becomes a new min;
%     basin A keeps value 3 (it is 8-adjacent to the marker, so the
%     erosion wave reaches it directly and only m caps from below) but
%     is no longer a regional minimum because (1,1)=2 is a smaller
%     neighbour. Basin B is erased (lifted to plateau 10). ---
BW2 = false(3, 7);
BW2(1, 1) = true;
J2 = imimposemin(I, BW2);
[~, n2] = bwlabel(imregionalmin(J2));
fprintf('--- imimposemin(I, BW only at (1,1)) ---\n');
fprintf('  regional-min count of J2 = %d (expect 1 — only the new marker)\n', n2);
fprintf('  J2(1,1) = %.1f (expect 2 — sentinel)\n', J2(1, 1));
fprintf('  J2(2,2) = %.1f (expect 3 — keeps value, but no longer a regional min)\n', ...
    J2(2, 2));
fprintf('  J2(2,5) = %.1f (expect 10 — basin B erased: lifted to boundary)\n\n', ...
    J2(2, 5));

% --- Mark BOTH minima. Both survive, no new ones. ---
BW3 = false(3, 7);
BW3(2, 2) = true; BW3(2, 5) = true;
J3 = imimposemin(I, BW3);
[~, n3] = bwlabel(imregionalmin(J3));
fprintf('--- imimposemin(I, BW at both minima) ---\n');
fprintf('  regional-min count of J3 = %d (expect 2 — both markers)\n', n3);
fprintf('  J3(2,2) = J3(2,5) = %.1f, %.1f (both sentinel = 2)\n', ...
    J3(2, 2), J3(2, 5));
