clear

import compat.*

% Same 3-blob layout as imclearborder smoke; imkeepborder should
% return the COMPLEMENT result: rim-touching components survive,
% interior components vanish.
BW = false(5, 7);
BW(1, 1) = true; BW(1, 2) = true;
BW(2, 1) = true; BW(2, 2) = true;
BW(3, 4) = true;
BW(2, 7) = true; BW(3, 7) = true; BW(4, 7) = true;

K = imkeepborder(BW);
[~, n] = bwlabel(K);
fprintf('--- imkeepborder, default conn=8 ---\n');
fprintf('  total fg before = %d, after = %d (expect 7 — interior dot stripped)\n', ...
    sum(BW(:)), sum(K(:)));
fprintf('  components after = %d (expect 2 — corner blob + right bar)\n', n);
fprintf('  K(1,1) = %d (expect 1 — kept)\n', K(1, 1));
fprintf('  K(2,7) = %d (expect 1 — kept)\n', K(2, 7));
fprintf('  K(3,4) = %d (expect 0 — interior stripped)\n\n', K(3, 4));

% --- Cross-check identity: imclearborder ⊕ imkeepborder = BW ---
% (Every FG pixel is either in a rim-touching component or not.)
J = imclearborder(BW);
delta = max(max(abs(double(BW) - (double(J) + double(K)))));
fprintf('--- duality check imclearborder + imkeepborder == BW ---\n');
fprintf('  max|BW - (J + K)| = %d (expect 0)\n\n', delta);

% --- conn-sensitivity: diagonal touch ---
BW2 = false(4, 4);
BW2(1, 1) = true; BW2(2, 2) = true;
K8 = imkeepborder(BW2, 8);
K4 = imkeepborder(BW2, 4);
fprintf('--- diagonal-touch conn sensitivity ---\n');
fprintf('  conn=8: total fg = %d (expect 2 — both kept, one big component)\n', ...
    sum(K8(:)));
fprintf('  conn=4: total fg = %d (expect 1 — only the rim pixel)\n', sum(K4(:)));
fprintf('  conn=4: K4(1,1)=%d K4(2,2)=%d (expect 1 0)\n\n', ...
    K4(1, 1), K4(2, 2));

% --- Degenerate ---
fprintf('--- degenerate cases ---\n');
fprintf('  imkeepborder(zeros) total = %d (expect 0)\n', ...
    sum(imkeepborder(false(4, 4))(:)));
fprintf('  imkeepborder(ones)  total = %d (expect 16 — everything touches)\n', ...
    sum(imkeepborder(true(4, 4))(:)));
