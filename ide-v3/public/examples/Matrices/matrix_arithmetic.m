% Matrix Arithmetic — Addition and element-wise multiply
% Demonstrates matrix addition and element-wise (.*) operations.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

A = [1 2 3; 4 5 6; 7 8 9];
B = [9 8 7; 6 5 4; 3 2 1];
disp(A + B)
disp(A .* B)
