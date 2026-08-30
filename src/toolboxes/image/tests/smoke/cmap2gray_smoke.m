clear

% cmap2gray — N×3 colormap → N×1 grayscale (Octave weights).

fprintf('--- pure RGB primaries ---\n');
M = [1 0 0; 0 1 0; 0 0 1];
g = cmap2gray(M);
fprintf('size: %s\n', mat2str(size(g)));
disp(g);
fprintf('  expect N×3: each row [y y y] for y = 0.298936/0.587043/0.114021\n\n');

fprintf('--- gray ramp [0 0 0; 0.5 0.5 0.5; 1 1 1] ---\n');
g = cmap2gray([0 0 0; 0.5 0.5 0.5; 1 1 1]);
disp(g);
fprintf('  expect: [0 0 0; 0.5 0.5 0.5; 1 1 1]  (sum of weights = 1)\n\n');

fprintf('--- jet-like 5-row palette ---\n');
M = [0 0 0.5; 0 0.5 1; 0.5 1 0.5; 1 0.5 0; 0.5 0 0];
g = cmap2gray(M);
disp(g);
