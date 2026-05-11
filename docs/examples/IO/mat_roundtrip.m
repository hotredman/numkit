% MAT Round-trip — save, load
% Write a matrix to a binary .mat file (MATLAB v5) and read it back.
% The .mat extension picks the binary backend automatically; use
% '-ascii' to fall back to plain text.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

A = magic(4);
save('demo.mat', 'A');

clear A
load('demo.mat');
disp(A)
