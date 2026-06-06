% CSV Write Offset — place data at a specific row/column
% csvwrite(file, M, R, C) writes M starting at row R, column C (0-based),
% filling the preceding cells with commas/newlines.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

csvwrite('offset.csv', [7 8; 9 10], 1, 2);

% Reading back shows the leading blanks filled with zeros
disp(csvread('offset.csv'))
