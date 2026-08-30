clear

% Build a 3x3 square SE. strel(...) returns a structuring-element struct;
% its mask is the .Neighborhood field (double(SE) is not defined on a struct).
SE = strel('square', 3);
fprintf('--- strel(''square'', 3) ---\n');
disp(SE.Neighborhood);
fprintf('  expect: 3×3 of 1s\n\n');

% Diamond r=2
fprintf('--- strel(''diamond'', 2) ---\n');
SEd = strel('diamond', 2);
disp(SEd.Neighborhood);
fprintf('  expect: diamond pattern\n\n');

% Disk r=2
fprintf('--- strel(''disk'', 2) ---\n');
SEk = strel('disk', 2);
disp(SEk.Neighborhood);
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
