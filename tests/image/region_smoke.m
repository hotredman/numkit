import compat.*

% Three disjoint blobs
A = [1 1 0 0 1 1;
     1 1 0 0 1 1;
     0 0 0 0 0 0;
     1 0 0 1 1 0;
     1 0 0 1 1 0];

[L, n] = bwlabel(A);
fprintf('--- bwlabel: components found = %d ---\n', n);
disp(L);
fprintf('  expect: 3 distinct components labeled 1, 2, 3\n\n');

% bwarea
fprintf('bwarea(A) = %.0f (expect 12)\n\n', bwarea(A));

% bwperim
fprintf('--- bwperim(A) ---\n');
disp(double(bwperim(A)));
fprintf('  expect: same as A (all foreground pixels are on the boundary in this small image)\n\n');

% bwareaopen — keep components with ≥ 4 pixels
fprintf('--- bwareaopen(A, 4) ---\n');
disp(double(bwareaopen(A, 4)));
fprintf('  expect: keeps the 4-pixel and the 4-pixel components, drops 1-pixel\n\n');

% bwconncomp
[c, sz, n2, p] = bwconncomp(A);
fprintf('bwconncomp: connectivity=%d, size=[%dx%d], num=%d\n', ...
    c, sz(1), sz(2), n2);
fprintf('  expect: connectivity=8, size=[5×6], num=3\n');
