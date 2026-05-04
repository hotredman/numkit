clear

import compat.*

% --- Donut: hollow ring with one center hole ---
BW = false(7, 7);
BW(2:6, 2:6) = true;
BW(3:5, 3:5) = false;   % dig out a 3x3 interior hole
fprintf('--- imfill on donut ---\n');
fprintf('  count(BW) before fill = %d (expect 25 - 9 = 16)\n', sum(BW(:)));
J = imfill(BW, 'holes');
fprintf('  count(J)  after fill  = %d (expect 25 — full square)\n', sum(J(:)));
fprintf('  J(4, 4)   = %d (expect 1 — interior hole filled)\n', J(4, 4));
fprintf('  J(1, 1)   = %d (expect 0 — outside background untouched)\n\n', ...
    J(1, 1));

% --- Two holes ---
BW2 = false(8, 10);
BW2(2:7, 2:9) = true;
BW2(3:4, 3:4) = false;   % hole 1
BW2(5:6, 6:8) = false;   % hole 2
fprintf('--- imfill on rectangle with 2 holes ---\n');
fprintf('  count(BW2) before = %d (expect 48 - 4 - 6 = 38)\n', sum(BW2(:)));
J2 = imfill(BW2, 'holes');
fprintf('  count(J2)  after  = %d (expect 48 — both holes filled)\n', sum(J2(:)));
fprintf('  hole 1 filled? %d (expect 1)\n', all(all(J2(3:4, 3:4))));
fprintf('  hole 2 filled? %d (expect 1)\n\n', all(all(J2(5:6, 6:8))));

% --- Hole touching the border is NOT a hole (it's outer background) ---
BW3 = true(5, 5);
BW3(:, 1) = false;       % open left edge — entire image's "0" region
                          % connects to the border via this opening, so
                          % anything connected to it is NOT a hole.
BW3(3, 3) = false;       % isolated interior hole (still surrounded by 1s)
fprintf('--- imfill where border-connected 0 region exists ---\n');
J3 = imfill(BW3, 'holes');
fprintf('  J3(3, 3) = %d (expect 1 — true interior hole filled)\n', J3(3, 3));
fprintf('  J3(:, 1) all 0? %d (expect 1 — border-connected ⇒ not a hole)\n\n', ...
    all(J3(:, 1) == 0));

% --- All-foreground: imfill returns identity ---
BW4 = true(4, 4);
J4 = imfill(BW4, 'holes');
fprintf('--- imfill on all-foreground ---\n');
fprintf('  identical? %d (expect 1)\n', isequal(J4, BW4));

% --- All-background: imfill returns identity ---
BW5 = false(4, 4);
J5 = imfill(BW5, 'holes');
fprintf('--- imfill on all-background ---\n');
fprintf('  identical? %d (expect 1)\n', isequal(J5, BW5));
