clear

% spring — magenta-to-yellow colormap.

fprintf('--- size(spring()) ---\n');
s = spring();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(s)));

fprintf('\n--- spring(5) ---\n');
disp(spring(5));
fprintf('  expect: r=1, g=0..1 step 0.25, b=1-g\n');

fprintf('\n--- spring(1) ---\n');
disp(spring(1));
fprintf('  expect [1 0 1]\n');

fprintf('\n--- spring(0) size ---\n');
fprintf('size spring(0)  = %s\n', mat2str(size(spring(0))));
