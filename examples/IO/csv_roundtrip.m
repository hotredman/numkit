% CSV Round-trip — csvwrite, csvread
% Write a numeric matrix to a comma-separated text file (CSV) and read it back.

clear

A = [1 2 3; 4 5 6; 7 8 9];
csvwrite('demo.csv', A);

B = csvread('demo.csv');
disp('Read back matrix:');
disp(B);
