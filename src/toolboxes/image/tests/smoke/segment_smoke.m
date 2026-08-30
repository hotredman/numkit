clear

% --- dice / jaccard textbook examples ---
A = false(4, 4);
A(2:3, 2:3) = true;     % 4 pixels
B = false(4, 4);
B(2:3, 3:4) = true;     % 4 pixels, overlaps with A in 2 pixels
fprintf('--- dice / jaccard on 2x2 vs 2x2 with 2-pixel overlap ---\n');
fprintf('  intersection = 2, union = 6\n');
d = dice(A, B);
j = jaccard(A, B);
fprintf('  dice    = %.4f (expect 2*2/(4+4) = 0.5)\n', d);
fprintf('  jaccard = %.4f (expect 2/6 = 0.3333)\n\n', j);

% --- Identical masks: dice = jaccard = 1 ---
fprintf('  dice(A, A)    = %.4f (expect 1.0)\n', dice(A, A));
fprintf('  jaccard(A, A) = %.4f (expect 1.0)\n\n', jaccard(A, A));

% --- Both empty: degenerate convention returns 1 ---
E = false(3, 3);
fprintf('  dice(empty, empty)    = %.4f (expect 1.0)\n', dice(E, E));
fprintf('  jaccard(empty, empty) = %.4f (expect 1.0)\n\n', jaccard(E, E));

% --- Disjoint masks: 0 ---
A2 = false(4, 4); A2(1:2, 1:2) = true;
B2 = false(4, 4); B2(3:4, 3:4) = true;
fprintf('  dice(disjoint)    = %.4f (expect 0)\n', dice(A2, B2));
fprintf('  jaccard(disjoint) = %.4f (expect 0)\n\n', jaccard(A2, B2));

% --- boundarymask on a label image with 2 abutting regions ---
% Make image bigger so we get a real "interior" away from image edges.
L = zeros(7, 9);
L(2:6, 2:5) = 1;  % left chunk  label 1
L(2:6, 6:8) = 2;  % right chunk label 2 (sharing col-5/6 edge)
bm = boundarymask(L);
fprintf('--- boundarymask on adjacent labelled regions ---\n');
fprintf('  size = %dx%d\n', size(bm, 1), size(bm, 2));
fprintf('  bm(4, 5) = %d (expect 1, region-1 pixel touching region 2)\n', bm(4, 5));
fprintf('  bm(4, 6) = %d (expect 1, region-2 pixel touching region 1)\n', bm(4, 6));
fprintf('  bm(4, 3) = %d (expect 0, fully interior of region 1)\n', bm(4, 3));
fprintf('  bm(2, 2) = %d (expect 1, region-1 pixel touching label-0 background)\n', bm(2, 2));
fprintf('  bm(1, 1) = %d (expect 0, all-background)\n\n', bm(1, 1));

% --- label2idx on a label image ---
L = zeros(3, 3);
L(1, 1) = 1; L(1, 2) = 1;
L(2, 2) = 2; L(2, 3) = 2; L(3, 3) = 2;
idx = label2idx(L);
fprintf('--- label2idx ---\n');
fprintf('  numel(idx) = %d (expect 2)\n', numel(idx));
fprintf('  idx{1} = '); disp(idx{1}');
fprintf('  expect [1 4]   (linear indices of label 1, col-major)\n');
fprintf('  idx{2} = '); disp(idx{2}');
fprintf('  expect [5 8 9] (linear indices of label 2)\n');

% --- Compose: bwlabel → label2idx → check consistency ---
BW = false(4, 4);
BW(1, 1:2) = true;       % blob 1
BW(3:4, 3:4) = true;     % blob 2
[L_lbl, n] = bwlabel(BW);
idx2 = label2idx(L_lbl);
fprintf('\n--- bwlabel + label2idx pipeline ---\n');
fprintf('  n = %d (expect 2)\n', n);
fprintf('  numel(idx2) = %d (expect 2)\n', numel(idx2));
fprintf('  numel(idx2{1}) = %d (expect 2 — first blob has 2 px)\n', ...
    numel(idx2{1}));
fprintf('  numel(idx2{2}) = %d (expect 4 — second blob has 4 px)\n', ...
    numel(idx2{2}));
