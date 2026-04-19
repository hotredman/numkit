% Manual CSV — fprintf each row yourself
% For full control over the format you can write CSV with fprintf
% instead of csvwrite, then read it back with csvread as usual.
clear

fid = fopen('data.csv', 'w');
fprintf(fid, '%d,%d,%d\n', 1, 2, 3);
fprintf(fid, '%d,%d,%d\n', 4, 5, 6);
fprintf(fid, '%d,%d,%d\n', 7, 8, 9);
fclose(fid);

M = csvread('data.csv');
disp(M)
