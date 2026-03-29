% Nested Loops — Multiplication table
% Build and display a 5x5 multiplication table row by row.
clear

for i = 1:5
    row = '';
    for j = 1:5
        row = [row, num2str(i*j), ' '];
    end
    disp(row)
end
