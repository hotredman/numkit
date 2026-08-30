clear

% pink — pastel pink colormap.

fprintf('--- size(pink()) ---\n');
p = pink();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(p)));

fprintf('\n--- pink(8) ---\n');
disp(pink(8));
fprintf('  expect MATLAB row 1: [0.3333 0 0]; row 8: [1 1 1]\n');

fprintf('\n--- pink(1) ---\n');
disp(pink(1));
fprintf('  expect sqrt([1/3 1/3 1/3]) ≈ 0.5774\n');

fprintf('\n--- pink(2) ---\n');
disp(pink(2));
fprintf('  expect [0.5774 0.5774 0.4082; 1 1 1]\n');
