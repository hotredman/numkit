% CSV Read Range — skip rows/cols or read a rectangle
% csvread(file, R1, C1) skips the first R1 rows and C1 columns (0-based).
% csvread(file, R1, C1, [R1 C1 R2 C2]) reads only the specified rectangle.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

csvwrite('grid.csv', [1 2 3 4; 5 6 7 8; 9 10 11 12; 13 14 15 16]);

% Skip the first row and first column
disp(csvread('grid.csv', 1, 1))

% Read only the 2x2 block at rows 1..2, cols 1..2
disp(csvread('grid.csv', 1, 1, [1 1 2 2]))
