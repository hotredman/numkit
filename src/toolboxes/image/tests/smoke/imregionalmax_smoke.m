clear

import compat.*

% --- Single peak: only that pixel is a regional max ---
I = uint8([
    10 10 10 10 10;
    10 20 30 20 10;
    10 30 50 30 10;
    10 20 30 20 10;
    10 10 10 10 10]);
M = imregionalmax(I);
fprintf('--- imregionalmax on single-peak image ---\n');
fprintf('  count(M) = %d (expect 1 — only the 50 peak)\n', sum(M(:)));
fprintf('  M(3, 3)  = %d (expect 1 — peak)\n', M(3, 3));
fprintf('  M(2, 2)  = %d (expect 0 — value 20 is not a max)\n\n', M(2, 2));

% --- Plateau: all pixels with the same max value, contiguous, are one regional max ---
I2 = uint8([
    10 10 10;
    10 50 50;
    10 50 10;
    10 10 10]);
M2 = imregionalmax(I2);
fprintf('--- imregionalmax on plateau ---\n');
fprintf('  count(M2) = %d (expect 3 — connected plateau of 50s)\n', sum(M2(:)));
fprintf('  M2(2, 2) = %d (expect 1)\n', M2(2, 2));
fprintf('  M2(2, 3) = %d (expect 1)\n', M2(2, 3));
fprintf('  M2(3, 2) = %d (expect 1)\n\n', M2(3, 2));

% --- Constant image: every pixel is part of a flat plateau which the
%     reconstruction-based formula reports as an entire regional max
%     (Vincent's 1993 definition). MATLAB shares this behaviour.
Ic = uint8(50 * ones(4, 4));
Mc = imregionalmax(Ic);
fprintf('--- imregionalmax on constant ---\n');
fprintf('  count(Mc) = %d / %d (expect all true — single plateau)\n\n', ...
    sum(Mc(:)), numel(Mc));

% --- imregionalmin on the negated image ---
% Inverted: max=255 (uint8 default class max). Single trough at center.
Itr = uint8([
    100 100 100 100 100;
    100  90  80  90 100;
    100  80  10  80 100;
    100  90  80  90 100;
    100 100 100 100 100]);
Mn = imregionalmin(Itr);
fprintf('--- imregionalmin on single-trough image ---\n');
fprintf('  count(Mn) = %d (expect 1 — only the 10 trough)\n', sum(Mn(:)));
fprintf('  Mn(3, 3) = %d (expect 1)\n', Mn(3, 3));

% --- Two peaks separated by valley ---
Iv = uint8([
    10 10 10 10 10;
    10 30 10 30 10;
    10 10 10 10 10]);
Mv = imregionalmax(Iv);
fprintf('\n--- imregionalmax on 2 peaks ---\n');
fprintf('  count(Mv) = %d (expect 2 — two disjoint 30s)\n', sum(Mv(:)));
fprintf('  Mv(2, 2) = %d, Mv(2, 4) = %d (expect both 1)\n', ...
    Mv(2, 2), Mv(2, 4));
