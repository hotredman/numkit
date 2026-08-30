clear

% hot — N×3 black-red-yellow-white colormap.

fprintf('--- size(hot()) ---\n');
h = hot();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(h)));

fprintf('\n--- hot(11) (Octave reference) ---\n');
disp(hot(11));

fprintf('\n--- hot(1) ---\n');
disp(hot(1));
fprintf('  expect [1 1 1]\n');

fprintf('\n--- hot(2) ---\n');
disp(hot(2));
fprintf('  expect [1 1 0.5; 1 1 1]\n');

fprintf('\n--- hot(3) ---\n');
disp(hot(3));
fprintf('  expect [1 0 0; 1 1 0; 1 1 1]\n');

fprintf('\n--- hot(0) size ---\n');
fprintf('size hot(0)  = %s\n', mat2str(size(hot(0))));
