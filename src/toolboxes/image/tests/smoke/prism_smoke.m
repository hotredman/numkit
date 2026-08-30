clear

% prism — cyclic 6-row rainbow palette.

fprintf('--- size(prism()) ---\n');
p = prism();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(p)));

fprintf('\n--- prism(7) (full cycle + 1) ---\n');
disp(prism(7));
fprintf('  expect cycle [r,o,y,g,b,v] then back to r at row 7\n');

fprintf('\n--- prism(1) ---\n');
disp(prism(1));
fprintf('  expect [1 0 0]\n');

fprintf('\n--- prism(0) size ---\n');
fprintf('size prism(0)  = %s\n', mat2str(size(prism(0))));
