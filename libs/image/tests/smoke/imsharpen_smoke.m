import compat.*

% --- imsharpen on a step-edge: amplify the edge contrast ---
% Smooth ramp followed by a step. Sharpening should make the step
% steeper (overshoot/undershoot near the edge).
I = double([
    1 1 1 1 5 5 5 5;
    1 1 1 1 5 5 5 5;
    1 1 1 1 5 5 5 5;
    1 1 1 1 5 5 5 5]);

B = imsharpen(I);   % defaults: radius=1, amount=0.8, threshold=0
fprintf('--- imsharpen(step) defaults ---\n');
fprintf('  I(1,4)=%.1f I(1,5)=%.1f (raw step: 1, 5)\n', I(1,4), I(1,5));
fprintf('  B(1,4)=%.2f B(1,5)=%.2f (expect overshoot — B(1,4)<1 and B(1,5)>5)\n', ...
    B(1,4), B(1,5));
fprintf('  edge contrast: I = %.2f, B = %.2f (expect B > I)\n\n', ...
    I(1,5)-I(1,4), B(1,5)-B(1,4));

% --- amount=0 should be identity ---
B0 = imsharpen(I, 'Amount', 0);
fprintf('--- imsharpen(I, ''Amount'', 0) — identity ---\n');
fprintf('  max|I - B0| = %.6e (expect 0)\n\n', max(max(abs(I - B0))));

% --- name-value 'Radius' alters the blur kernel ---
B_small = imsharpen(I, 'Radius', 0.5, 'Amount', 1);
B_large = imsharpen(I, 'Radius', 3,   'Amount', 1);
fprintf('--- radius scan ---\n');
fprintf('  small-r overshoot at (1,5): %.2f, large-r overshoot: %.2f\n', ...
    B_small(1,5)-5, B_large(1,5)-5);
fprintf('  (large-r should reach more pixels but lower per-pixel overshoot)\n\n');

% --- threshold suppresses weak edges ---
J = double([
    1 1 1 1 5 5 5 5;
    1 1 1 1 5 5 5 5;
    1 1 1 1 5 5 5 5;
    1 1 1 1 5 5 5 5;
    1 1 1 1 1 1 1 1;
    1 1 1 1 1 1 1 1]) ...
    + 0.1 * (mod(reshape(1:48, [6 8]), 7) - 3);  % small noise
B_thr = imsharpen(J, 'Amount', 1, 'Threshold', 0.99);
B_no  = imsharpen(J, 'Amount', 1, 'Threshold', 0);
fprintf('--- threshold suppresses weak edges ---\n');
fprintf('  no-thr  variance: %.4f\n', var(B_no(:) - J(:)));
fprintf('  hi-thr  variance: %.4f (expect smaller)\n\n', var(B_thr(:) - J(:)));

% --- im2bw smoke ---
% Match imbinarize behaviour: im2bw(I) uses Otsu, im2bw(I, level) thresholds.
G = double([0 0.2 0.4 0.6 0.8 1.0]);
BW1 = im2bw(G, 0.5);
fprintf('--- im2bw alias check ---\n');
fprintf('  im2bw([.0 .2 .4 .6 .8 1], 0.5) = [%d %d %d %d %d %d] (expect 0 0 0 1 1 1)\n', ...
    BW1(1), BW1(2), BW1(3), BW1(4), BW1(5), BW1(6));
% Otsu fallback: bimodal data
G2 = [zeros(1, 50), ones(1, 50)];
BW2 = im2bw(G2);
fprintf('  im2bw(bimodal) sum = %d (expect 50 — Otsu lands between modes)\n', ...
    sum(BW2(:)));
