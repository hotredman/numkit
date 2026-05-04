clear

import compat.*

% Tiny synthetic image
I = uint8([0 50 100 150 200 250]);

% imhist
fprintf('--- imhist(I, 8) ---\n');
[c, x] = imhist(I, 8);
fprintf('counts = '); disp(c');
fprintf('bins   = '); disp(x');
fprintf('  expect: 6 ones (one per bin) since I has 6 distinct values across 8 bins\n');
fprintf('          actually each maps to nearest bin so distribution depends on quantisation\n\n');

% stretchlim default tol
fprintf('--- stretchlim(I) ---\n');
disp(stretchlim(I));
fprintf('  expect: ~ [low; high] near [0.0; 1.0] (tiny set, 1%% & 99%% percentiles)\n\n');

% imadjust with explicit input range
J = uint8([50 100 150 200]);
fprintf('--- imadjust(J, [0.2 0.8], [0 1], 1) ---\n');
disp(double(imadjust(J, [0.2 0.8], [0 1], 1)));
fprintf('  expect: linearly stretch [0.2..0.8] → [0..1] then map to uint8\n');
fprintf('          50/255=0.196 < 0.2 → 0\n');
fprintf('          100/255=0.392 → (0.392-0.2)/0.6 = 0.32 → 82\n');
fprintf('          150/255=0.588 → (0.588-0.2)/0.6 = 0.647 → 165\n');
fprintf('          200/255=0.784 → (0.784-0.2)/0.6 = 0.973 → 248\n\n');

% imadjust with gamma
fprintf('--- imadjust(J, [0 1], [0 1], 0.5) (sqrt brightening) ---\n');
disp(double(imadjust(J, [0 1], [0 1], 0.5)));
fprintf('  expect: brightened (sqrt of normalised value)\n\n');

% histeq on a skewed distribution
K = uint8([0 0 0 50 100 150 200 250]);  % heavy in dark
fprintf('--- histeq(K, 8) ---\n');
disp(double(histeq(K, 8)));
fprintf('  expect: redistributes towards uniform — dark values spread out\n');
