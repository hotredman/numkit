% CSV Read Range — skip rows/cols or read a bounded rectangle
% csvread(file, R1, C1) skips the first R1 rows and C1 columns (0-based indexing).
% csvread(file, R1, C1, [R1 C1 R2 C2]) reads only the specified rectangular region.

clear

grid = [ 1  2  3  4;
         5  6  7  8;
         9 10 11 12;
        13 14 15 16 ];
csvwrite('grid.csv', grid);

% 1. Skip the first row and first column (reading submatrix starting at (1,1))
sub1 = csvread('grid.csv', 1, 1);
disp('Submatrix skipping first row and column (from row 1, col 1):');
disp(sub1);

% 2. Read only the 2x2 block at rows 1..2, cols 1..2
block = csvread('grid.csv', 1, 1, [1 1 2 2]);
disp('2x2 block [rows 1..2, cols 1..2]:');
disp(block);
