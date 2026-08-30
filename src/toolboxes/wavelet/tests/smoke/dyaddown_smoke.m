clear

% dyaddown — dyadic downsampling.
% ODD = 0 (default) → x(2:2:end);  ODD = 1 → x(1:2:end)

fprintf('=== ODD = 0 (default, 1-based even positions) ===\n');
disp(dyaddown([10 20 30 40 50 60 70]));
fprintf('  expect: [20 40 60]\n\n');

fprintf('=== ODD = 1 ===\n');
disp(dyaddown([10 20 30 40 50 60 70], 1));
fprintf('  expect: [10 30 50 70]\n\n');

fprintf('=== column orientation preserved ===\n');
y = dyaddown([1; 2; 3; 4]);
fprintf('  size = %dx%d (expect 2x1)\n', size(y, 1), size(y, 2));
disp(y);
