clear

% labelmatrix + cc2bw — CC struct conversions.
BW = logical([1 1 0 0 0;
              1 1 0 1 1;
              0 0 0 1 0;
              0 1 1 0 0;
              0 1 0 0 1]);
CC = bwconncomp(BW, 4);
fprintf('CC.NumObjects=%d\n', CC.NumObjects);

fprintf('\n=== labelmatrix(CC) ===\n');
L = labelmatrix(CC);
fprintf('class=%s  L(1,1)=%d (expect 1)  L(2,4)=%d (expect 3)  L(4,2)=%d (expect 2)  L(5,5)=%d (expect 4)\n', ...
    class(L), L(1,1), L(2,4), L(4,2), L(5,5));

fprintf('\n=== cc2bw(CC) (default = original BW) ===\n');
BW2 = cc2bw(CC);
fprintf('class=%s  isequal(BW2, BW)=%d (expect 1)\n', class(BW2), isequal(BW2, BW));

fprintf('\n=== cc2bw(CC, ObjectsToKeep=2) ===\n');
B = cc2bw(CC, 'ObjectsToKeep', 2);
fprintf('B(4,2)=%d B(5,2)=%d B(4,3)=%d B(1,1)=%d (expect 1 1 1 0)\n', ...
    B(4,2), B(5,2), B(4,3), B(1,1));

fprintf('\n=== cc2bw(CC, ObjectsToKeep=[1 3]) ===\n');
B = cc2bw(CC, 'ObjectsToKeep', [1 3]);
fprintf('B(1,1)=%d B(2,4)=%d B(4,2)=%d (expect 1 1 0)\n', ...
    B(1,1), B(2,4), B(4,2));

fprintf('\n=== cc2bw logical vector ===\n');
lv = false(1, CC.NumObjects);
lv(1) = true; lv(end) = true;
B = cc2bw(CC, 'ObjectsToKeep', lv);
fprintf('B(1,1)=%d B(5,5)=%d B(4,2)=%d (expect 1 1 0)\n', ...
    B(1,1), B(5,5), B(4,2));

fprintf('\n=== empty CC ===\n');
CC0 = bwconncomp(false(3, 3));
L0 = labelmatrix(CC0);
fprintf('class=%s NumObjects=%d sum(L0(:))=%d\n', class(L0), CC0.NumObjects, sum(L0(:)));

fprintf('\n=== single object → uint8 output ===\n');
CC1 = bwconncomp(BW > 0.5 & BW < 1.5);  % full BW
L1 = labelmatrix(bwconncomp(true(3,3), 8));
fprintf('class(L1) = %s\n', class(L1));
