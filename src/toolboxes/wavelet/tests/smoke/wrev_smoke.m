clear

% wrev: reverse a vector (Wavelet Toolbox helper).
% Same as fliplr/flipud on a vector but kept for MATLAB-style code.

fprintf('=== row vector ===\n');
y = wrev([1 2 3 4 5]);
disp(y);
fprintf('  expect: [5 4 3 2 1]\n\n');

fprintf('=== mixed signs / doubles ===\n');
y = wrev([1.5 -2 0 7 -1.5]);
disp(y);
fprintf('  expect: [-1.5  7  0  -2  1.5]\n\n');

fprintf('=== column vector preserves orientation ===\n');
y = wrev([1; 2; 3]);
fprintf('  size = %dx%d (expect 3x1)\n', size(y, 1), size(y, 2));
disp(y);

fprintf('=== scalar ===\n');
y = wrev(42);
fprintf('  y = %d (expect 42)\n', y);

fprintf('=== empty ===\n');
y = wrev([]);
fprintf('  numel = %d (expect 0)\n', numel(y));

fprintf('=== round-trip wrev(wrev(x)) == x ===\n');
x = [3.14 -2.7 1.41 0 9.9];
y = wrev(wrev(x));
fprintf('  max diff: %.4e (expect 0)\n', max(abs(y - x)));
