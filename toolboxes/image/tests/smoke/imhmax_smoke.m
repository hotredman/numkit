clear

import compat.*

% --- Two peaks of different heights: shallow one suppressed by h ---
%       40           — peak A (height 30 above plateau 10)
%        |
%       20           — peak B (height 10 above plateau 10)
%        |
%   ---10---10---10  — background plateau
I = double([
    10 10 10 10 10 10 10;
    10 40 10 10 20 10 10;
    10 10 10 10 10 10 10]);

% h=5: both peaks survive (both > 5 deep)
J5 = imhmax(I, 5);
[~, n5] = bwlabel(imregionalmax(J5));
fprintf('--- imhmax(I, 5): regional-max count after suppression ---\n');
fprintf('  count = %d (expect 2 — both peaks deeper than h=5)\n\n', n5);

% h=15: peak B (height 10) suppressed; peak A (height 30) survives
J15 = imhmax(I, 15);
[~, n15] = bwlabel(imregionalmax(J15));
fprintf('--- imhmax(I, 15) ---\n');
fprintf('  count = %d (expect 1 — only peak A survives)\n', n15);
fprintf('  J15 at peak A (2,2) = %.1f (expect 25 = 40 − h, top shaved)\n', ...
    J15(2, 2));
fprintf('  J15 at peak B (2,5) = %.1f (expect 10 — flattened to plateau)\n\n', ...
    J15(2, 5));

% h=50: both peaks suppressed
J50 = imhmax(I, 50);
[~, n50] = bwlabel(imregionalmax(J50));
fprintf('--- imhmax(I, 50) — way deeper than any peak ---\n');
fprintf('  count = %d (expect ≤ 1 — flat-or-near-flat result)\n\n', n50);

% --- imhmin: dual on a trough image ---
% Two troughs of different depths. Background = 100.
% Trough A at (2,2) drops to 10 (depth 90).
% Trough B at (2,5) drops to 70 (depth 30).
T = double([
    100 100 100 100 100 100 100;
    100  10 100 100  70 100 100;
    100 100 100 100 100 100 100]);

J_h20 = imhmin(T, 20);
[~, c20] = bwlabel(imregionalmin(J_h20));
fprintf('--- imhmin(T, 20) ---\n');
fprintf('  trough count = %d (expect 2 — both deeper than h=20)\n', c20);

J_h50 = imhmin(T, 50);
[~, c50] = bwlabel(imregionalmin(J_h50));
fprintf('  imhmin(T, 50) — trough count = %d (expect 1 — A only, B flattened)\n', ...
    c50);
fprintf('  J_h50(2, 5) = %.1f (expect 100 — trough B raised to background)\n', ...
    J_h50(2, 5));

% --- h=0 is identity ---
I0 = imhmax(I, 0);
fprintf('\n--- imhmax(I, 0) — identity ---\n');
fprintf('  max|I - I0| = %.6e (expect 0)\n', max(max(abs(I - I0))));
