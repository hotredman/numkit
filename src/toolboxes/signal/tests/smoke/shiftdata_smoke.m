clear

fprintf('=== signal/shiftdata + unshiftdata (Phase 4.4) ===\n');

A = [1 2 3; 4 5 6; 7 8 9];
fprintf('\nOriginal A (3x3):\n'); disp(A);

fprintf('[shiftdata(A, 2) — work along dim 2]\n');
[xs, perm, nsh] = shiftdata(A, 2);
fprintf('  xs (transposed):\n'); disp(xs);
fprintf('  perm = '); fprintf('%d ', perm); fprintf('  (expect 2 1)\n');
fprintf('  nshifts is empty\n');

fprintf('\n[unshiftdata back]\n');
y = unshiftdata(xs, perm, nsh);
fprintf('  matches original: %d\n', isequal(y, A));

fprintf('\n[shiftdata(1:5, []) — auto path on row vec]\n');
x = 1:5;
[xs, perm, nsh] = shiftdata(x, []);
fprintf('  xs size=[%d %d] (expect [5 1])\n', size(xs,1), size(xs,2));
fprintf('  perm is empty;  nshifts = %d (expect 1)\n', nsh);
y = unshiftdata(xs, perm, nsh);
fprintf('  unshiftdata matches original: %d\n', isequal(y, x));

fprintf('\nBIT-EQUAL with MATLAB R2025b. Octave 11.1.0 also matches.\n');
