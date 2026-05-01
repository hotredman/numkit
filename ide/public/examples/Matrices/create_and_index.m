% Create & Index — Matrix creation and element access
% Build a 3x3 matrix and access individual elements.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

A = [1 2 3; 4 5 6; 7 8 9];
disp(A)
disp(A(2, 3))
