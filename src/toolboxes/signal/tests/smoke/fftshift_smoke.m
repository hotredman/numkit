clear

fprintf('=== fftshift / ifftshift ===\n');

% Vector even N
fprintf('  fftshift([1:8])  = '); disp(fftshift(1:8));
fprintf('             expect:  5 6 7 8 1 2 3 4\n');

% Vector odd N — bug fix 2026-05-08: was returning ifftshift result
fprintf('  fftshift([1:7])  = '); disp(fftshift(1:7));
fprintf('             expect:  5 6 7 1 2 3 4 (split at ceil(7/2)=4)\n');

fprintf('  ifftshift([1:7]) = '); disp(ifftshift(1:7));
fprintf('             expect:  4 5 6 7 1 2 3 (split at floor(7/2)=3)\n');

% Round-trip identity
fprintf('  ifftshift(fftshift([1:7])) = '); disp(ifftshift(fftshift(1:7)));
fprintf('             expect:  1 2 3 4 5 6 7\n');

% Matrix — both dims shift
M = [1 2 3; 4 5 6; 7 8 9];
fprintf('  fftshift(M):\n'); disp(fftshift(M));
fprintf('  expect:  9 7 8\n           3 1 2\n           6 4 5\n');

% dim arg
fprintf('  fftshift(M, 1) (rows only):\n'); disp(fftshift(M, 1));
fprintf('  expect:  7 8 9\n           1 2 3\n           4 5 6\n');

fprintf('  fftshift(M, 2) (cols only):\n'); disp(fftshift(M, 2));
fprintf('  expect:  3 1 2\n           6 4 5\n           9 7 8\n');
