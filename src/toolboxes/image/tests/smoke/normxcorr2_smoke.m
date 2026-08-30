clear

% normxcorr2 — normalized cross-correlation for template matching.

% --- perfect-match self-correlation: peak should be 1.0 ---
fprintf('--- perfect match: peak = ?\n');
A    = double([1 2 3 4 5; 2 4 6 8 10; 3 5 7 9 11; 4 6 8 10 12]);
T    = A(2:3, 3:4);
C    = normxcorr2(T, A);
[v, ~] = max(C(:));
fprintf('size(C) = %s, peak = %.6f (expect ~ 1)\n', mat2str(size(C)), v);

% --- output range bounded by [-1, 1] ---
fprintf('\n--- range check ---\n');
fprintf('min/max of C = [%.4f, %.4f]\n', min(C(:)), max(C(:)));

% --- bug #50122: constant-region image gives 0 (not Inf) ---
fprintf('\n--- bug #50122 (no Inf on constant region) ---\n');
img  = [1 1 1 0];
t    = [1 1 0];
c    = normxcorr2(t, img);
fprintf('c(3) = %g (expect 0; was Inf in old impl)\n', c(3));

% --- shift invariance under additive constant ---
fprintf('\n--- shift invariance ---\n');
a    = [1 2 3; 4 5 6; 7 8 9];
b    = [1 2 3 4; 5 6 7 8; 9 10 11 12];
c1   = normxcorr2(a, b);
c2   = normxcorr2(a + 100, b + 50);
fprintf('max|c1 - c2| = %.4e (expect ~0)\n', max(abs(c1(:) - c2(:))));
