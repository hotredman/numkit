clear

% dyadup — zero-insertion (dyadic upsampling).
% default ODD = 1: [0 x(1) 0 x(2) 0 ... x(N) 0]   length 2N+1
% ODD = 0:        [x(1) 0 x(2) 0 ... x(N)]        length 2N-1

fprintf('=== ODD = 1 (default) ===\n');
disp(dyadup([1 2 3]));
fprintf('  expect: [0 1 0 2 0 3 0]\n\n');

fprintf('=== ODD = 0 ===\n');
disp(dyadup([1 2 3], 0));
fprintf('  expect: [1 0 2 0 3]\n\n');

fprintf('=== column orientation preserved ===\n');
y = dyadup([1; 2; 3]);
fprintf('  size = %dx%d (expect 7x1)\n', size(y, 1), size(y, 2));
disp(y);
