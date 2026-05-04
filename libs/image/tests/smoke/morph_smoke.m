clear

import compat.*

% Build a 3x3 square SE
SE = strel('square', 3);
fprintf('--- strel(''square'', 3) ---\n');
disp(double(SE));
fprintf('  expect: 3×3 of 1s\n\n');

% Diamond r=2
fprintf('--- strel(''diamond'', 2) ---\n');
disp(double(strel('diamond', 2)));
fprintf('  expect: diamond pattern\n\n');

% Disk r=2
fprintf('--- strel(''disk'', 2) ---\n');
disp(double(strel('disk', 2)));
fprintf('  expect: 5×5 disk-like pattern\n\n');

% Erosion of a binary image with single-pixel hole
A = [0 0 0 0 0;
     0 1 1 1 0;
     0 1 1 1 0;
     0 1 1 1 0;
     0 0 0 0 0];
fprintf('--- imerode(A, 3x3 square SE) ---\n');
disp(double(imerode(A, SE)));
fprintf('  expect: only centre pixel survives → [0...; 0 0 0 0 0; 0 0 1 0 0; 0 0 0 0 0; 0...]\n\n');

fprintf('--- imdilate(A, 3x3 square SE) ---\n');
disp(double(imdilate(A, SE)));
fprintf('  expect: 5x5 of all 1s (3x3 region grew to fill image)\n\n');

% Open removes thin features, close fills small holes
B = [0 0 0 0 0;
     0 1 0 1 0;
     0 1 1 1 0;
     0 1 0 1 0;
     0 0 0 0 0];
fprintf('--- imopen(B, square 3) ---\n');
disp(double(imopen(B, SE)));
fprintf('  expect: removes "corners", keeps central core\n\n');

C = [1 1 1 1 1;
     1 1 0 1 1;
     1 0 0 0 1;
     1 1 0 1 1;
     1 1 1 1 1];
fprintf('--- imclose(C, square 3) ---\n');
disp(double(imclose(C, SE)));
fprintf('  expect: holes filled → all 1s\n');
