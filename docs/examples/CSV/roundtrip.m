% CSV Round-trip — csvwrite, csvread
% Write a matrix to a CSV file and read it back.
clear

A = [1 2 3; 4 5 6; 7 8 9];
csvwrite('demo.csv', A);

B = csvread('demo.csv');
disp(B)
