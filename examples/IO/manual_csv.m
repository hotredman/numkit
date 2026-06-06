% Manual CSV — fprintf each row yourself
% For full control over the format you can write CSV with fprintf
% instead of csvwrite, then read it back with csvread as usual.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

fid = fopen('data.csv', 'w');
fprintf(fid, '%d,%d,%d\n', 1, 2, 3);
fprintf(fid, '%d,%d,%d\n', 4, 5, 6);
fprintf(fid, '%d,%d,%d\n', 7, 8, 9);
fclose(fid);

M = csvread('data.csv');
disp(M)
