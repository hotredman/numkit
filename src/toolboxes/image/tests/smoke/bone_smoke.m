clear

% bone — grayscale-with-blue-tint colormap.

fprintf('--- size(bone()) ---\n');
b = bone();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(b)));

fprintf('\n--- bone(8) (Octave reference) ---\n');
disp(bone(8));
fprintf('  expect: row1=[0 0 0.0417], row8=[1 1 1]\n');

fprintf('\n--- bone(1) ---\n');
disp(bone(1));
fprintf('  expect [0.125 0.125 0.125]\n');

fprintf('\n--- bone(2) ---\n');
disp(bone(2));
fprintf('  expect [0.0625 0.125 0.125; 1 1 1]\n');

fprintf('\n--- bone(0) size ---\n');
fprintf('size bone(0)  = %s\n', mat2str(size(bone(0))));
