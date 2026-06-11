clear

import compat.*

% --- Reconstruct one connected component from a single seed ---
% Mask has two disconnected blobs; marker only seeds one of them.
% Result should equal the seeded blob (filled), other blob = 0.
mask = false(7, 9);
mask(2:3, 2:3) = true;     % blob A (top-left)
mask(5:6, 6:8) = true;     % blob B (bottom-right)
marker = false(7, 9);
marker(2, 2) = true;       % seed inside blob A only
J = imreconstruct(marker, mask);
fprintf('--- imreconstruct: seed inside blob A, two-blob mask ---\n');
fprintf('  count(J)        = %d (expect 4 — blob A only)\n', sum(J(:)));
fprintf('  count(mask)     = %d (expect 4 + 6 = 10)\n', sum(mask(:)));
fprintf('  J(2:3, 2:3) all 1? %d (expect 1)\n', all(all(J(2:3, 2:3))));
fprintf('  J(5:6, 6:8) all 0? %d (expect 1 — blob B unreached)\n\n', ...
    all(all(~J(5:6, 6:8))));

% --- Marker == mask: reconstruction is the mask itself ---
J2 = imreconstruct(mask, mask);
fprintf('--- imreconstruct(mask, mask) ---\n');
fprintf('  match? %d (expect 1)\n\n', isequal(J2, mask));

% --- Empty marker yields empty output ---
J3 = imreconstruct(false(5, 5), true(5, 5));
fprintf('--- imreconstruct(zeros, ones) ---\n');
fprintf('  count(J) = %d (expect 0 — nothing to grow from)\n\n', sum(J3(:)));

% --- Grayscale: marker capped, then dilated up to mask ceiling ---
mask = uint8([
    0  0  0  0  0;
    0 50 60 70  0;
    0 60 80 70  0;
    0 50 60 70  0;
    0  0  0  0  0]);
marker = uint8(zeros(5, 5));
marker(3, 3) = 80;  % seed only at the central peak
Jg = imreconstruct(marker, mask);
fprintf('--- grayscale imreconstruct ---\n');
fprintf('  Jg(3, 3) = %d (expect 80 — seed peak)\n', Jg(3, 3));
fprintf('  Jg(2, 3) = %d (expect 60 — capped at mask)\n', Jg(2, 3));
fprintf('  Jg(2, 4) = %d (expect 70 — capped at mask)\n', Jg(2, 4));
fprintf('  Jg(1, 1) = %d (expect 0 — outside mask support)\n', Jg(1, 1));
