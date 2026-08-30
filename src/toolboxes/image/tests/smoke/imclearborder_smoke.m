clear

% Three blobs:
%   A — 2x2 in the corner (touches rim)         → should be removed
%   B — 1x1 interior                              → should remain
%   C — 1x3 along the right edge                 → should be removed
BW = false(5, 7);
BW(1, 1) = true; BW(1, 2) = true;
BW(2, 1) = true; BW(2, 2) = true;
BW(3, 4) = true;                          % isolated interior
BW(2, 7) = true; BW(3, 7) = true; BW(4, 7) = true;

J = imclearborder(BW);
[~, n] = bwlabel(J);
fprintf('--- imclearborder, default conn=8 ---\n');
fprintf('  total fg before = %d, after = %d\n', sum(BW(:)), sum(J(:)));
fprintf('  components after = %d (expect 1 — only the interior dot)\n', n);
fprintf('  J(3,4) = %d (expect 1)\n', J(3, 4));
fprintf('  J(1,1) = %d (expect 0 — corner blob removed)\n', J(1, 1));
fprintf('  J(2,7) = %d (expect 0 — right-edge bar removed)\n\n', J(2, 7));

% --- 4-connectivity edge case ---
% A diagonal touch: foreground at (1,1) and (2,2) only.
% Under conn=8, both are one component touching the rim — both go.
% Under conn=4, (2,2) is interior and not connected to (1,1) → keep.
BW2 = false(4, 4);
BW2(1, 1) = true; BW2(2, 2) = true;

J8  = imclearborder(BW2, 8);
J4  = imclearborder(BW2, 4);
fprintf('--- diagonal-touch conn sensitivity ---\n');
fprintf('  conn=8: total fg = %d (expect 0 — both removed)\n', sum(J8(:)));
fprintf('  conn=4: total fg = %d (expect 1 — interior survives)\n', sum(J4(:)));
fprintf('  conn=4: J4(2,2) = %d (expect 1)\n\n', J4(2, 2));

% --- All-zero / all-one inputs ---
fprintf('--- degenerate cases ---\n');
fprintf('  imclearborder(zeros)  total = %d (expect 0)\n', ...
    sum(imclearborder(false(4, 4))(:)));
fprintf('  imclearborder(ones)   total = %d (expect 0 — all touches rim)\n', ...
    sum(imclearborder(true(4, 4))(:)));
