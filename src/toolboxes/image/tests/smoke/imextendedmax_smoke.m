clear

% Two peaks of different heights against a plateau:
%   peak A height 30, peak B height 10.
% imextendedmax(I, h) keeps regional maxima of imhmax(I, h),
% i.e. peaks at least h above their surroundings.
I = double([
    10 10 10 10 10 10 10;
    10 40 10 10 20 10 10;
    10 10 10 10 10 10 10]);

% h=5 — both peaks tall enough → both flagged.
M5 = imextendedmax(I, 5);
[~, n5] = bwlabel(M5);
fprintf('--- imextendedmax(I, 5) ---\n');
fprintf('  count = %d (expect 2 — both deeper than h=5)\n', n5);
fprintf('  M5(2,2)=%d M5(2,5)=%d (expect 1 1)\n\n', M5(2,2), M5(2,5));

% h=15 — only peak A (height 30) survives.
M15 = imextendedmax(I, 15);
[~, n15] = bwlabel(M15);
fprintf('--- imextendedmax(I, 15) ---\n');
fprintf('  count = %d (expect 1 — peak A only)\n', n15);
fprintf('  M15(2,2)=%d M15(2,5)=%d (expect 1 0)\n\n', M15(2,2), M15(2,5));

% h=50 — both peaks suppressed. imhmax flattens to uniform; on a flat
% image the Vincent formula flags every pixel (one big component).
M50 = imextendedmax(I, 50);
[~, n50] = bwlabel(M50);
fprintf('--- imextendedmax(I, 50) ---\n');
fprintf('  count = %d (expect 1 — uniform after flatten ⇒ one big region)\n', n50);
fprintf('  all(M50(:)) = %d (expect 1)\n\n', all(M50(:)));

% --- imextendedmin: dual on a trough image ---
T = double([
    100 100 100 100 100 100 100;
    100  10 100 100  70 100 100;
    100 100 100 100 100 100 100]);

% Trough A depth 90, trough B depth 30.
N20 = imextendedmin(T, 20);
[~, c20] = bwlabel(N20);
fprintf('--- imextendedmin(T, 20) ---\n');
fprintf('  count = %d (expect 2 — both troughs deeper than h=20)\n', c20);

N50 = imextendedmin(T, 50);
[~, c50] = bwlabel(N50);
fprintf('--- imextendedmin(T, 50) ---\n');
fprintf('  count = %d (expect 1 — A only)\n', c50);
fprintf('  N50(2,2)=%d N50(2,5)=%d (expect 1 0)\n\n', N50(2,2), N50(2,5));

% h=0 should give regional max (every regional max passes the trivial test).
M0 = imextendedmax(I, 0);
R0 = imregionalmax(I);
fprintf('--- imextendedmax(I, 0) vs imregionalmax(I) ---\n');
fprintf('  max|M0 - R0| = %d (expect 0 — h=0 collapses to regional max)\n', ...
    max(max(abs(double(M0) - double(R0)))));
