% CSV Round-trip — csvwrite, csvread
% Write a matrix to a CSV file and read it back.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

A = [1 2 3; 4 5 6; 7 8 9];
csvwrite('demo.csv', A);

B = csvread('demo.csv');
disp(B)
