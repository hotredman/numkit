clear

import compat.*

% --- Single foreground pixel at center: D should be Euclidean dist ---
BW = false(5, 5);
BW(3, 3) = true;     % center
D = bwdist(BW);
fprintf('--- bwdist on single-point mask (5x5, center) ---\n');
fprintf('  D(3,3) = %.6f (expect 0)\n', D(3, 3));
fprintf('  D(1,1) = %.6f (expect sqrt(8) = 2.8284)\n', D(1, 1));
fprintf('  D(1,3) = %.6f (expect 2)\n', D(1, 3));
fprintf('  D(3,1) = %.6f (expect 2)\n', D(3, 1));
fprintf('  D(2,2) = %.6f (expect sqrt(2) = 1.4142)\n', D(2, 2));
fprintf('  D(5,5) = %.6f (expect sqrt(8) = 2.8284)\n\n', D(5, 5));

% --- Foreground = whole top row: dist is just row distance ---
BW2 = false(5, 5);
BW2(1, :) = true;
D2 = bwdist(BW2);
fprintf('--- bwdist on full top row ---\n');
fprintf('  D2(1, :) = '); disp(D2(1, :));
fprintf('  expect all 0 (top row is foreground)\n');
fprintf('  D2(:, 1) = '); disp(D2(:, 1)');
fprintf('  expect [0 1 2 3 4]\n\n');

% --- Two foreground pixels: distance to nearest ---
BW3 = false(5, 5);
BW3(1, 1) = true;
BW3(5, 5) = true;
D3 = bwdist(BW3);
fprintf('--- bwdist on diagonal corners ---\n');
fprintf('  D3(3, 3) = %.6f (expect sqrt(8) = 2.8284 — same dist to both)\n', D3(3, 3));
fprintf('  D3(2, 2) = %.6f (expect sqrt(2) — closer to (1,1))\n', D3(2, 2));
fprintf('  D3(4, 4) = %.6f (expect sqrt(2) — closer to (5,5))\n\n', D3(4, 4));

% --- Empty mask: D = +Inf everywhere ---
BWe = false(3, 3);
De = bwdist(BWe);
fprintf('--- bwdist on empty mask ---\n');
fprintf('  D(1,1) = %s (expect Inf)\n', mat2str(De(1, 1)));
fprintf('  isfinite(D(2,2)) = %d (expect 0)\n', isfinite(De(2, 2)));

% --- Sanity: max distance in HxW grid never exceeds sqrt(H^2+W^2) ---
% Random binary mask, check max(D) ≤ √(H²+W²)
rng(42);
BWr = rand(8, 12) > 0.95;
% Force at least one foreground pixel
if sum(BWr(:)) == 0
    BWr(1, 1) = true;
end
Dr = bwdist(BWr);
fprintf('\n--- random mask 8x12 ---\n');
fprintf('  count(BW) = %d\n', sum(BWr(:)));
fprintf('  max(D) = %.4f (expect ≤ sqrt(64+144) = 14.42)\n', max(Dr(:)));
fprintf('  min(D) = %.4f (expect 0 since at least 1 foreground pixel)\n', ...
    min(Dr(:)));
