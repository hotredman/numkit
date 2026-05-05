% Nested Loops — Multiplication table
% Build and display a 5x5 multiplication table row by row.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

for i = 1:5
    row = '';
    for j = 1:5
        row = [row, num2str(i*j), ' '];
    end
    disp(row)
end
