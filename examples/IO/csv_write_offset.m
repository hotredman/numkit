% CSV Write Offset — place data at a specific row/column offset
% csvwrite(file, M, R, C) writes M starting at 0-based row R, column C,
% filling preceding empty cells with commas/zeros.

clear

M = [7 8; 9 10];
csvwrite('offset.csv', M, 1, 2);

% Reading back shows the leading blanks filled with zeros
result = csvread('offset.csv');
disp('Matrix written at offset row=1, col=2:');
disp(result);
